#include "TeamsTreeModel.h"
#include "ContactIcons.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "Filter.h"
#include "Person.h"
#include "StatusIcons.h"
#include "Team.h"

#include <QColor>
#include <QFont>
#include <algorithm>

namespace
{

/// A team member's row: the person's id, and the name the row shows. The leader sorts first.
struct MemberRow
{
    PersonId personId;
    QString displayName;
    bool isLeader = false;
};

bool leaderFirstThenAlpha(const MemberRow& a, const MemberRow& b)
{
    if (a.isLeader != b.isLeader)
    {
        return a.isLeader;
    }
    return a.displayName.toLower() < b.displayName.toLower();
}

bool byTeamName(const Team& a, const Team& b)
{
    return a.name().toLower() < b.name().toLower();
}

/// "Category - Family - Description", naming the family as the task recorded it, then
/// whether its assignee was told when it has one, then "[RESOLVED]" when it is resolved.
QString taskText(const ResponseTask& task)
{
    QString text = task.category()
        + " - " + task.familyName()
        + " - \"" + task.description() + "\"";
    if (task.isAssigned())
    {
        text += task.isNotified()
            ? " " + StatusIcons::Checkmark + " " + QObject::tr("notified")
            : " " + StatusIcons::Bullet + " " + QObject::tr("not notified");
    }
    if (task.isResolved())
    {
        text += " [" + QObject::tr("RESOLVED") + "]";
    }
    return text;
}

/// The tasks of the emergency in progress that sit under teamId's row: those assigned to
/// the team, or with no team, those with no assignment at all. In the emergency's order.
QList<ResponseTask> tasksUnder(const std::optional<TeamId>& teamId)
{
    QList<ResponseTask> tasks;
    for (const ResponseTask& task : EmergencyManager::instance()->response().tasks())
    {
        bool under = teamId ? task.isAssigned() && task.assignment()->teamId() == teamId : !task.isAssigned();
        if (under)
        {
            tasks.append(task);
        }
    }
    return tasks;
}

QString unassignedHeaderText(int count)
{
    return QObject::tr("Unassigned Tasks (%1)").arg(count);
}

}  // namespace

TeamsTreeModel::TeamsTreeModel(Filter* filter,
                               QObject* parent)
    : BaseTreeModel(parent)
    , m_filter(filter)
{
    // Rebuilt on every change; the view keeps its selection, expansion and scroll.
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &TeamsTreeModel::rebuild);
    if (m_filter)
    {
        connect(m_filter, &Filter::changed, this, &TeamsTreeModel::rebuild);
    }
    rebuild();
}

TeamsTreeModel::~TeamsTreeModel()
{
    clearNodes();
}

void TeamsTreeModel::clearNodes()
{
    qDeleteAll(m_teamNodes);
    m_teamNodes.clear();
}

void TeamsTreeModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = DocumentManager::instance()->document();
    QList<Team> teams = doc.teams().values();
    bool emergencyActive = EmergencyManager::instance()->isActive();
    std::sort(teams.begin(), teams.end(), &byTeamName);

    for (const Team& team : teams)
    {
        // Collect and sort members (with filtering)
        QList<MemberRow> members;
        std::optional<PersonId> leaderId = team.leaderId();
        for (const PersonId& personId : team.memberIds())
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (!person || (m_filter && !m_filter->passes(doc, *person)))
            {
                continue;
            }
            MemberRow row;
            row.personId = personId;
            row.isLeader = leaderId == personId;
            row.displayName = person->displayName() + formatContactSuffix(*person);
            if (row.isLeader)
            {
                row.displayName += tr(" (leader)");
            }
            members.append(row);
        }
        std::sort(members.begin(), members.end(), &leaderFirstThenAlpha);

        // Display: "Team Name (N)"
        TreeNode* teamNode = new TreeNode();
        teamNode->type = ItemType::Team;
        teamNode->teamId = team.id();
        teamNode->displayText = tr("%1 (%2)")
            .arg(team.name())
            .arg(members.size());

        for (const MemberRow& member : members)
        {
            TreeNode* memberNode = new TreeNode();
            memberNode->type = ItemType::TeamMember;
            memberNode->personId = member.personId;
            memberNode->teamId = team.id();
            memberNode->displayText = member.displayName;
            memberNode->parent = teamNode;
            teamNode->children.append(memberNode);
        }

        // During emergencies, the tasks assigned to this team follow its members
        if (emergencyActive)
        {
            for (const ResponseTask& task : tasksUnder(team.id()))
            {
                teamNode->children.append(newTaskNode(task, teamNode));
            }
        }

        m_teamNodes.append(teamNode);
    }

    // During emergencies, the tasks with no assignment at all
    if (emergencyActive)
    {
        QList<ResponseTask> unassigned = tasksUnder(std::nullopt);
        if (!unassigned.isEmpty())
        {
            m_teamNodes.append(newUnassignedHeader(unassigned));
        }
    }

    endResetModel();
}

TeamsTreeModel::TreeNode* TeamsTreeModel::newTaskNode(const ResponseTask& task, TreeNode* parent)
{
    TreeNode* taskNode = new TreeNode();
    taskNode->type = ItemType::TaskRow;
    taskNode->teamId = parent->teamId;
    taskNode->parent = parent;
    taskNode->taskId = task.id();
    taskNode->familyId = task.familyId();
    taskNode->taskResolved = task.isResolved();
    taskNode->displayText = taskText(task);
    return taskNode;
}

TeamsTreeModel::TreeNode* TeamsTreeModel::newUnassignedHeader(const QList<ResponseTask>& tasks)
{
    TreeNode* header = new TreeNode();
    header->type = ItemType::UnassignedTasksHeader;
    header->displayText = unassignedHeaderText(tasks.size());
    for (const ResponseTask& task : tasks)
    {
        header->children.append(newTaskNode(task, header));
    }
    return header;
}

TeamsTreeModel::TreeNode* TeamsTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}

QModelIndex TeamsTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        if (row >= 0 && row < m_teamNodes.size())
        {
            return createIndex(row, 0, m_teamNodes.at(row));
        }
        return QModelIndex();
    }

    TreeNode* parentNode = nodeFromIndex(parent);
    if (!parentNode)
    {
        return QModelIndex();
    }

    if (row >= 0 && row < parentNode->children.size())
    {
        return createIndex(row, 0, parentNode->children.at(row));
    }

    return QModelIndex();
}

QModelIndex TeamsTreeModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // If parent is a team node (top-level)
    int teamRow = m_teamNodes.indexOf(parentNode);
    if (teamRow >= 0)
    {
        return createIndex(teamRow, 0, parentNode);
    }

    // Parent is a member node - find its row within the team
    if (parentNode->parent)
    {
        int memberRow = parentNode->parent->children.indexOf(parentNode);
        if (memberRow >= 0)
        {
            return createIndex(memberRow, 0, parentNode);
        }
    }

    return QModelIndex();
}

int TeamsTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return m_teamNodes.size();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return node->children.size();
    }

    return 0;
}

int TeamsTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool TeamsTreeModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_teamNodes.isEmpty();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (!node)
    {
        return false;
    }

    // Member nodes can have contact children (lazy loaded)
    if (node->type == ItemType::TeamMember)
    {
        if (node->contactsLoaded)
        {
            return !node->children.isEmpty();
        }
        return true;  // Not yet loaded, assume yes
    }

    return !node->children.isEmpty();
}

QVariant TeamsTreeModel::data(const QModelIndex& index, int role) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QVariant();
    }

    switch (role)
    {
    case Qt::DisplayRole:
        return node->displayText;

    case Qt::FontRole:
    {
        if (node->type == ItemType::TaskRow && node->taskResolved)
        {
            QFont font;
            font.setStrikeOut(true);
            return font;
        }
        return QVariant();
    }

    case Qt::ForegroundRole:
    {
        if (node->type == ItemType::TaskRow)
        {
            return QColor(Qt::darkGray);
        }
        return QVariant();
    }

    case ItemTypeRole:
        return QVariant::fromValue(node->type);

    case TeamIdRole:
    {
        if (node->teamId)
        {
            return node->teamId->toString();
        }
        return QVariant();
    }

    case TaskIdRole:
    {
        if (node->taskId)
        {
            return node->taskId->toString();
        }
        return QVariant();
    }

    case FamilyIdRole:
    {
        if (node->familyId)
        {
            return node->familyId->toString();
        }
        return QVariant();
    }

    default:
        return QVariant();
    }
}

SelectionKey TeamsTreeModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return SelectionKey::literal(QString());
    }

    switch (node->type)
    {
    case ItemType::Team:
    case ItemType::UnassignedTasksHeader:
        if (node->teamId)
        {
            return SelectionKey::from(*node->teamId);
        }
        return SelectionKey::literal("unassigned-tasks");
    case ItemType::TeamMember:
        if (node->personId && node->teamId)
        {
            return SelectionKey::literal(node->teamId->toString() + ":" + node->personId->toString());
        }
        if (node->teamId)
        {
            return SelectionKey::from(*node->teamId);
        }
        return SelectionKey::literal(QString());
    case ItemType::TaskRow:
        if (node->taskId)
        {
            return SelectionKey::literal("task:" + node->taskId->toString());
        }
        return SelectionKey::literal(QString());
    case ItemType::ContactDetail:
        return selectionKeyAt(index.parent());
    default:
        return SelectionKey::literal(QString());
    }
}

FamilyAssociation TeamsTreeModel::relatedFamiliesAt(const QModelIndex& index) const
{
    FamilyAssociation assoc;
    if (!index.isValid())
    {
        return assoc;
    }

    const Document& doc = DocumentManager::instance()->document();
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return assoc;
    }

    switch (node->type)
    {
    case ItemType::Team:
    {
        if (!node->teamId)
        {
            break;
        }
        std::optional<Team> teamOpt = doc.findTeamById(*node->teamId);
        if (teamOpt)
        {
            for (const PersonId& personId : teamOpt->memberIds())
            {
                std::optional<FamilyId> familyId = doc.familyIdForPerson(personId);
                if (familyId)
                {
                    assoc.relatedFamilyIds.insert(*familyId);
                }
            }
            // Leader's family as contact point
            if (teamOpt->leaderId())
            {
                std::optional<FamilyId> leaderFamilyId = doc.familyIdForPerson(*teamOpt->leaderId());
                if (leaderFamilyId)
                {
                    assoc.contactPointFamilyIds.insert(*leaderFamilyId);
                }
            }
        }
        break;
    }
    case ItemType::TeamMember:
    {
        if (node->personId)
        {
            std::optional<FamilyId> familyId = doc.familyIdForPerson(*node->personId);
            if (familyId)
            {
                assoc.relatedFamilyIds.insert(*familyId);
            }
        }
        break;
    }
    case ItemType::TaskRow:
    {
        if (node->familyId)
        {
            assoc.relatedFamilyIds.insert(*node->familyId);
        }
        break;
    }
    default:
        break;
    }

    return assoc;
}

ItemType TeamsTreeModel::itemTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return ItemType::Invalid;
}

std::optional<TeamId> TeamsTreeModel::teamIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return std::nullopt;
    }
    return node->teamId;
}

std::optional<PersonId> TeamsTreeModel::personIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return std::nullopt;
    }
    return node->personId;
}

QModelIndex TeamsTreeModel::indexForFamilyId(const FamilyId& familyId) const
{
    const Document& doc = DocumentManager::instance()->document();

    for (int t = 0; t < m_teamNodes.size(); ++t)
    {
        TreeNode* teamNode = m_teamNodes[t];
        for (int m = 0; m < teamNode->children.size(); ++m)
        {
            TreeNode* memberNode = teamNode->children[m];
            if (memberNode->type == ItemType::TeamMember && memberNode->personId)
            {
                std::optional<FamilyId> fid = doc.familyIdForPerson(*memberNode->personId);
                if (fid && *fid == familyId)
                {
                    return createIndex(m, 0, memberNode);
                }
            }
        }
    }
    return {};
}

void TeamsTreeModel::loadContactDetails(const QModelIndex& index)
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->contactsLoaded)
    {
        return;
    }

    if (node->type != ItemType::TeamMember)
    {
        return;
    }

    const Document& doc = DocumentManager::instance()->document();
    if (!node->personId)
    {
        node->contactsLoaded = true;
        return;
    }

    std::optional<Person> person = doc.findPersonById(*node->personId);
    if (!person)
    {
        node->contactsLoaded = true;
        return;
    }

    // Check for address availability
    std::optional<FamilyId> familyId = doc.familyIdForPerson(*node->personId);
    const QHash<FamilyId, Family>& families = doc.families();
    bool hasAddress = false;
    if (familyId && families.contains(*familyId))
    {
        hasAddress = !families[*familyId].address().isEmpty();
    }

    // Count items to insert
    int itemCount = 0;
    if (!person->phone().isEmpty())
    {
        itemCount++;
    }
    if (!person->altPhone().isEmpty())
    {
        itemCount++;
    }
    if (!person->email().isEmpty())
    {
        itemCount++;
    }
    if (hasAddress)
    {
        itemCount++;
    }

    if (itemCount == 0)
    {
        node->contactsLoaded = true;
        return;
    }

    int insertRow = node->children.size();
    beginInsertRows(index, insertRow, insertRow + itemCount - 1);

    if (!person->phone().isEmpty())
    {
        TreeNode* phoneNode = new TreeNode();
        phoneNode->type = ItemType::ContactDetail;
        phoneNode->personId = node->personId;
        phoneNode->teamId = node->teamId;
        phoneNode->displayText = ContactIcons::Phone + person->phone();
        phoneNode->parent = node;
        node->children.append(phoneNode);
    }

    if (!person->altPhone().isEmpty())
    {
        TreeNode* altPhoneNode = new TreeNode();
        altPhoneNode->type = ItemType::ContactDetail;
        altPhoneNode->personId = node->personId;
        altPhoneNode->teamId = node->teamId;
        altPhoneNode->displayText = ContactIcons::Phone + person->altPhone() + tr(" (alt)");
        altPhoneNode->parent = node;
        node->children.append(altPhoneNode);
    }

    if (!person->email().isEmpty())
    {
        TreeNode* emailNode = new TreeNode();
        emailNode->type = ItemType::ContactDetail;
        emailNode->personId = node->personId;
        emailNode->teamId = node->teamId;
        emailNode->displayText = ContactIcons::Email + person->email();
        emailNode->parent = node;
        node->children.append(emailNode);
    }

    if (hasAddress)
    {
        const Family& family = families[*familyId];
        TreeNode* addrNode = new TreeNode();
        addrNode->type = ItemType::ContactDetail;
        addrNode->personId = node->personId;
        addrNode->teamId = node->teamId;
        addrNode->displayText = ContactIcons::Address + family.address().full();
        addrNode->parent = node;
        node->children.append(addrNode);
    }

    endInsertRows();
    node->contactsLoaded = true;
}
