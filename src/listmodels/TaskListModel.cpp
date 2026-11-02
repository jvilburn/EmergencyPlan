#include "TaskListModel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyCommands.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"

#include <QHash>

#include <algorithm>

namespace
{

const Document& document()
{
    return DocumentManager::instance()->document();
}

/// Who the task is assigned to, "(removed)" when the ward no longer holds them, or empty
/// when it is unassigned.
QString assigneeName(const ResponseTask& task)
{
    const std::optional<TaskAssignment>& assignment = task.assignment();
    if (assignment && assignment->teamId())
    {
        std::optional<Team> team = document().findTeamById(*assignment->teamId());
        return team ? team->name() : QObject::tr("(removed)");
    }
    if (assignment && assignment->personId())
    {
        std::optional<Person> person = document().findPersonById(*assignment->personId());
        return person ? person->displayName() : QObject::tr("(removed)");
    }
    return QString();
}

}  // namespace

TaskListModel::TaskListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &TaskListModel::onDocumentChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyStarted,
            this, &TaskListModel::rebuild);
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyEnded,
            this, &TaskListModel::rebuild);
    connect(EmergencyManager::instance(), &EmergencyManager::familyStatusChanged,
            this, &TaskListModel::replaceFamilyRows);
    rebuild();
}

int TaskListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_entries.size();
}

int TaskListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return ColumnCount;
}

QVariant TaskListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
    {
        return QVariant();
    }

    const TaskEntry& entry = m_entries.at(index.row());

    if (role == Qt::CheckStateRole && index.column() == ResolvedCol)
    {
        return entry.resolved ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case AssignedToCol:
            return entry.assigneeName;
        case CategoryCol:
            return entry.category;
        case FamilyCol:
            return entry.familyName;
        case DescriptionCol:
            return entry.description;
        default:
            return QVariant();
        }
    }

    return QVariant();
}

QVariant TaskListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return QVariant();
    }

    switch (section)
    {
    case ResolvedCol:
        return QString();
    case AssignedToCol:
        return tr("Assigned To");
    case CategoryCol:
        return tr("Category");
    case FamilyCol:
        return tr("Family");
    case DescriptionCol:
        return tr("Description");
    default:
        return QVariant();
    }
}

Qt::ItemFlags TaskListModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.column() == ResolvedCol)
    {
        f |= Qt::ItemIsUserCheckable;
    }
    return f;
}

bool TaskListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::CheckStateRole || index.column() != ResolvedCol)
    {
        return false;
    }

    EmergencyManager* manager = EmergencyManager::instance();
    if (!manager->isActive())
    {
        return false;
    }
    const ResponseTask* task = manager->response().findTask(m_entries.at(index.row()).taskId);
    if (!task)
    {
        return false;
    }

    // Resolving from the check box records no notes; clearing it reopens the task
    ResponseTask opened = *task;
    ResponseTask saved = opened;
    if (value.toInt() == Qt::Checked)
    {
        saved.setResolution(TaskResolution::create(QString()));
    }
    else
    {
        saved.setResolution(std::nullopt);
    }
    CommitResult result = DocumentManager::instance()->executeCommand(UpdateTaskCommand::fromEdit(opened, saved, document()));
    return result.status == CommitStatus::Applied;
}

FamilyId TaskListModel::familyIdForRow(int row) const
{
    return m_entries.at(row).familyId;
}

TaskId TaskListModel::taskIdForRow(int row) const
{
    return m_entries.at(row).taskId;
}

bool TaskListModel::isAssignedForRow(int row) const
{
    return m_entries.at(row).assigned;
}

bool TaskListModel::isNotifiedForRow(int row) const
{
    return m_entries.at(row).notified;
}

void TaskListModel::onDocumentChanged(const DocumentChange& change)
{
    if (change.action == ChangeAction::Full)
    {
        rebuild();
        return;
    }

    // A family or team renamed renames the rows that show it
    EmergencyManager* manager = EmergencyManager::instance();
    if ((change.familyId || change.teamId) && manager->isActive())
    {
        for (const FamilyId& familyId : manager->response().familyIds())
        {
            replaceFamilyRows(familyId);
        }
    }
}

void TaskListModel::rebuild()
{
    beginResetModel();
    m_entries.clear();

    EmergencyManager* manager = EmergencyManager::instance();
    if (manager->isActive())
    {
        for (const ResponseTask& task : manager->response().tasks())
        {
            m_entries.append(entryFor(task));
        }
        std::sort(m_entries.begin(), m_entries.end(), &TaskListModel::taskEntryLessThan);
    }

    endResetModel();
}

void TaskListModel::replaceFamilyRows(const FamilyId& familyId)
{
    EmergencyManager* manager = EmergencyManager::instance();
    if (!manager->isActive())
    {
        return;
    }
    QHash<TaskId, TaskEntry> wanted;
    for (const ResponseTask& task : manager->response().tasksFor(familyId))
    {
        wanted.insert(task.id(), entryFor(task));
    }

    // The family's rows whose task is gone go; the others take the task as it stands, and
    // move to their place
    for (int row = m_entries.size() - 1; row >= 0; --row)
    {
        if (m_entries.at(row).familyId == familyId && !wanted.contains(m_entries.at(row).taskId))
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_entries.removeAt(row);
            endRemoveRows();
        }
    }
    for (auto it = wanted.begin(); it != wanted.end();)
    {
        int row = 0;
        while (row < m_entries.size() && m_entries.at(row).taskId != it.key())
        {
            ++row;
        }
        if (row == m_entries.size())
        {
            ++it;
            continue;
        }

        m_entries[row] = it.value();
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));

        // Its place among the other rows, which are in order
        TaskEntry entry = m_entries.takeAt(row);
        int target = sortedRow(entry);
        m_entries.insert(row, entry);
        if (target != row)
        {
            beginMoveRows(QModelIndex(), row, row, QModelIndex(), target > row ? target + 1 : target);
            m_entries.move(row, target);
            endMoveRows();
        }
        it = wanted.erase(it);
    }

    // What is left in wanted are the family's new tasks
    for (const TaskEntry& entry : wanted)
    {
        int row = sortedRow(entry);
        beginInsertRows(QModelIndex(), row, row);
        m_entries.insert(row, entry);
        endInsertRows();
    }
}

TaskListModel::TaskEntry TaskListModel::entryFor(const ResponseTask& task)
{
    std::optional<Family> family = document().findFamilyById(task.familyId());

    TaskEntry entry;
    entry.familyId = task.familyId();
    entry.familyName = family ? family->displayName() : tr("(removed)");
    entry.taskId = task.id();
    entry.category = task.category();
    entry.description = task.description();
    entry.assigned = task.isAssigned();
    entry.assigneeName = assigneeName(task);
    entry.resolved = task.isResolved();
    entry.notified = task.isNotified();
    return entry;
}

int TaskListModel::sortedRow(const TaskEntry& entry) const
{
    return static_cast<int>(std::upper_bound(m_entries.begin(), m_entries.end(), entry, &TaskListModel::taskEntryLessThan)
                            - m_entries.begin());
}

// Sort: unresolved first, then assigned before unassigned, then assignee, category and family
bool TaskListModel::taskEntryLessThan(const TaskEntry& a, const TaskEntry& b)
{
    if (a.resolved != b.resolved)
    {
        return !a.resolved;
    }
    if (a.assigned != b.assigned)
    {
        return a.assigned;
    }
    int assigneeCmp = a.assigneeName.compare(b.assigneeName, Qt::CaseInsensitive);
    if (assigneeCmp != 0)
    {
        return assigneeCmp < 0;
    }
    int catCmp = a.category.compare(b.category, Qt::CaseInsensitive);
    if (catCmp != 0)
    {
        return catCmp < 0;
    }
    return a.familyName.compare(b.familyName, Qt::CaseInsensitive) < 0;
}
