#include "TaskListModel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "Person.h"
#include "Team.h"

TaskListModel::TaskListModel(DocumentManager* documentManager,
                             EmergencyManager* emergencyManager,
                             QObject* parent)
    : QAbstractTableModel(parent)
    , m_documentManager(documentManager)
    , m_emergencyManager(emergencyManager)
{
    connect(m_emergencyManager, &EmergencyManager::responseDataChanged,
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
            return entry.assignedTo;
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

    const TaskEntry& entry = m_entries.at(index.row());
    bool checked = (value.toInt() == Qt::Checked);

    if (checked)
    {
        m_emergencyManager->resolveTask(entry.familyId, entry.taskId, QString());
    }
    else
    {
        m_emergencyManager->reopenTask(entry.familyId, entry.taskId);
    }

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

void TaskListModel::rebuild()
{
    beginResetModel();
    m_entries.clear();

    if (!m_emergencyManager->isActive())
    {
        endResetModel();
        return;
    }

    const EmergencyResponse& response = m_emergencyManager->response();
    const QHash<FamilyId, FamilyResponseRecord>& records = response.familyRecords();
    const QHash<FamilyId, Family>& families = m_documentManager->document().families();

    for (auto it = records.constBegin(); it != records.constEnd(); ++it)
    {
        const FamilyId& familyId = it.key();
        const FamilyResponseRecord& record = it.value();

        if (record.tasks().isEmpty())
        {
            continue;
        }

        QString familyName;
        auto famIt = families.constFind(familyId);
        if (famIt != families.constEnd())
        {
            familyName = famIt.value().headOfHousehold();
        }

        for (const ResponseTask& task : record.tasks())
        {
            TaskEntry entry;
            entry.familyId = familyId;
            entry.familyName = familyName;
            entry.taskId = task.id();
            entry.category = task.category();
            entry.description = task.description();
            entry.assignedTo = resolveAssignedName(task.assignedTeamId(), task.assignedPersonId());
            entry.resolved = task.isResolved();
            m_entries.append(entry);
        }
    }

    // Sort: unresolved first, then assignedTo, then category, then family
    std::sort(m_entries.begin(), m_entries.end(),
              [](const TaskEntry& a, const TaskEntry& b)
              {
                  if (a.resolved != b.resolved)
                  {
                      return !a.resolved;  // unresolved first
                  }
                  // Assigned before unassigned, then alphabetical
                  bool aAssigned = !a.assignedTo.isEmpty();
                  bool bAssigned = !b.assignedTo.isEmpty();
                  if (aAssigned != bAssigned)
                  {
                      return aAssigned;
                  }
                  if (aAssigned && bAssigned)
                  {
                      int cmp = a.assignedTo.compare(b.assignedTo, Qt::CaseInsensitive);
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
              });

    endResetModel();
}

QString TaskListModel::resolveAssignedName(const std::optional<TeamId>& teamId,
                                           const std::optional<PersonId>& personId) const
{
    if (teamId)
    {
        auto it = m_documentManager->document().teams().constFind(*teamId);
        if (it != m_documentManager->document().teams().constEnd())
        {
            return it.value().name();
        }
    }
    if (personId)
    {
        const QHash<FamilyId, Family>& families = m_documentManager->document().families();
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
