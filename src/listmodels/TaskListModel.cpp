#include "TaskListModel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyCommands.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "Person.h"
#include "Team.h"

TaskListModel::TaskListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    connect(EmergencyManager::instance(), &EmergencyManager::responseDataChanged,
            this, &TaskListModel::rebuild);
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
            return resolveAssignedName(entry.assignedTeamId, entry.assignedPersonId);
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
    const Document& document = DocumentManager::instance()->document();
    DocumentManager::instance()->executeCommand(UpdateTaskCommand::fromEdit(*manager->shownId(), opened, saved, document));

    // rebuild() will be triggered by responseDataChanged signal
    return true;
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
    const TaskEntry& entry = m_entries.at(row);
    return entry.assignedTeamId.has_value() || entry.assignedPersonId.has_value();
}

bool TaskListModel::isNotifiedForRow(int row) const
{
    return m_entries.at(row).notified;
}

void TaskListModel::rebuild()
{
    beginResetModel();
    m_entries.clear();

    if (!EmergencyManager::instance()->isActive())
    {
        endResetModel();
        return;
    }

    const QHash<FamilyId, Family>& families = DocumentManager::instance()->document().families();
    for (const ResponseTask& task : EmergencyManager::instance()->response().tasks())
    {
        auto family = families.constFind(task.familyId());
        const std::optional<TaskAssignment>& assignment = task.assignment();

        TaskEntry entry;
        entry.familyId = task.familyId();
        entry.familyName = family != families.constEnd() ? family.value().displayName() : tr("(removed)");
        entry.taskId = task.id();
        entry.category = task.category();
        entry.description = task.description();
        entry.assignedTeamId = assignment ? assignment->teamId() : std::nullopt;
        entry.assignedPersonId = assignment ? assignment->personId() : std::nullopt;
        entry.resolved = task.isResolved();
        entry.notified = task.isNotified();
        m_entries.append(entry);
    }

    std::sort(m_entries.begin(), m_entries.end(), &TaskListModel::taskEntryLessThan);

    endResetModel();
}

// Sort: unresolved first, then assignedTo, then category, then family
bool TaskListModel::taskEntryLessThan(const TaskEntry& a, const TaskEntry& b)
{
    if (a.resolved != b.resolved)
    {
        return !a.resolved;  // unresolved first
    }
    // Assigned before unassigned, then alphabetical
    QString aName = resolveAssignedName(a.assignedTeamId, a.assignedPersonId);
    QString bName = resolveAssignedName(b.assignedTeamId, b.assignedPersonId);
    bool aAssigned = !aName.isEmpty();
    bool bAssigned = !bName.isEmpty();
    if (aAssigned != bAssigned)
    {
        return aAssigned;
    }
    if (aAssigned && bAssigned)
    {
        int cmp = aName.compare(bName, Qt::CaseInsensitive);
        if (cmp != 0)
        {
            return cmp < 0;
        }
    }
    int catCmp = a.category.compare(b.category, Qt::CaseInsensitive);
    if (catCmp != 0)
    {
        return catCmp < 0;
    }
    return a.familyName.compare(b.familyName, Qt::CaseInsensitive) < 0;
}

QString TaskListModel::resolveAssignedName(const std::optional<TeamId>& teamId,
                                           const std::optional<PersonId>& personId)
{
    const Document& doc = DocumentManager::instance()->document();
    if (teamId)
    {
        auto it = doc.teams().constFind(*teamId);
        if (it != doc.teams().constEnd())
        {
            return it.value().name();
        }
    }
    if (personId)
    {
        const QHash<FamilyId, Family>& families = doc.families();
        for (auto it = families.constBegin(); it != families.constEnd(); ++it)
        {
            for (const Person& person : it.value().members())
            {
                if (person.id() == *personId)
                {
                    return person.displayName();
                }
            }
        }
    }
    return QString();
}
