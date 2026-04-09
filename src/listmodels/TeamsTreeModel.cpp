#include "TeamsTreeModel.h"
#include "ContactIcons.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "Filter.h"
#include "Person.h"
#include "Team.h"

#include <QFont>
#include <algorithm>

namespace
{

const QString kCheckmark = "\u2713";
const QString kBullet = "\u2022";

bool leaderFirstThenAlpha(const QPair<PersonId, QString>& a,
                          const QPair<PersonId, QString>& b,
                          const std::optional<PersonId>& leaderId)
{
    bool aIsLeader = leaderId && *leaderId == a.first;
    bool bIsLeader = leaderId && *leaderId == b.first;
    if (aIsLeader != bIsLeader)
    {
        return aIsLeader;
    }
    return a.second.toLower() < b.second.toLower();
}

}  // namespace

TeamsTreeModel::TeamsTreeModel(Filter* filter,
                               QObject* parent)
    : BaseTreeModel(parent)
    , m_filter(filter)
{
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &TeamsTreeModel::onDocumentChanged);
    if (m_filter)
    {
        connect(m_filter, &Filter::changed, this, &TeamsTreeModel::rebuild);
    }
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyStarted,
            this, &TeamsTreeModel::onEmergencyStateChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyEnded,
            this, &TeamsTreeModel::onEmergencyStateChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::archiveViewOpened,
            this, &TeamsTreeModel::onEmergencyStateChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::archiveViewClosed,
            this, &TeamsTreeModel::onEmergencyStateChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::responseDataChanged,
            this, &TeamsTreeModel::rebuild);
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

void TeamsTreeModel::onDocumentChanged(const DocumentChange& change)
{
    // Full document reload or team changes: rebuild
    if (change.action == ChangeAction::Full || change.teamId)
    {
        rebuild();
        return;
    }

    // Family updated: refresh display text only
    if (change.familyId && change.action == ChangeAction::Updated)
    {
        refreshFamilyDisplayText(*change.familyId);
        return;
    }

    // Family added/removed: rebuild (member names may have changed)
    if (change.familyId)
    {
        rebuild();
    }
}

void TeamsTreeModel::onEmergencyStateChanged()
{
    rebuild();
}

void TeamsTreeModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = DocumentManager::instance()->document();
    QList<Team> teams = doc.teams().values();
    bool emergencyActive = EmergencyManager::instance() && EmergencyManager::instance()->isActive();

    // Sort teams by name
    std::sort(teams.begin(), teams.end(),
              [](const Team& a, const Team& b)
              { return a.name().toLower() < b.name().toLower(); });

    for (const Team& team : teams)
    {
        // Collect and sort members (with filtering)
        QList<QPair<PersonId, QString>> members;  // (personId, displayName)
        std::optional<PersonId> leaderId = team.leaderId();

        for (const PersonId& personId : team.memberIds())
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (person)
            {
                if (m_filter && !m_filter->passes(doc, *person))
                {
                    continue;
                }
                QString displayName = person->displayName() + formatContactSuffix(*person);
                if (leaderId && *leaderId == personId)
                {
                    displayName += tr(" (leader)");
                }
                members.append({personId, displayName});
            }
        }

        // Sort: leader first, then alphabetically
        std::sort(members.begin(), members.end(),
                  [&leaderId](const QPair<PersonId, QString>& a, const QPair<PersonId, QString>& b)
                  { return leaderFirstThenAlpha(a, b, leaderId); });

        // Create team node
        TreeNode* teamNode = new TreeNode();
        teamNode->type = ItemType::Team;
        teamNode->teamId = team.id();

        // Display: "Team Name (N)" with color swatch if set
        teamNode->displayText = QString("%1 (%2)")
            .arg(team.name())
            .arg(members.size());

        // Create member nodes
        for (const QPair<PersonId, QString>& memberData : members)
        {
            TreeNode* memberNode = new TreeNode();
            memberNode->type = ItemType::TeamMember;
            memberNode->personId = memberData.first;
            memberNode->teamId = team.id();
            memberNode->displayText = memberData.second;
            memberNode->parent = teamNode;
            teamNode->children.append(memberNode);
        }

        // During emergencies, add task rows assigned to this team
        if (emergencyActive)
        {
            const EmergencyResponse& response = EmergencyManager::instance()->response();
            const QHash<FamilyId, FamilyResponseRecord>& records = response.familyRecords();

            for (auto it = records.constBegin(); it != records.constEnd(); ++it)
            {
                const FamilyId& famId = it.key();
                const FamilyResponseRecord& record = it.value();

                for (const ResponseTask& task : record.tasks())
                {
                    if (task.assignedTeamId() && *task.assignedTeamId() == team.id())
                    {
                        TreeNode* taskNode = new TreeNode();
                        taskNode->type = ItemType::TaskRow;
                        taskNode->teamId = team.id();
                        taskNode->taskId = task.id();
                        taskNode->familyId = famId;
                        taskNode->taskResolved = task.isResolved();
                        taskNode->parent = teamNode;

                        // Format: "Category - Family - Description ✓ notified [RESOLVED]"
                        QString text = task.category()
                            + " - " + record.displayName()
                            + " - \"" + task.description() + "\"";

                        if (task.isNotified())
                        {
                            text += " " + kCheckmark + " " + tr("notified");
                        }
                        else
                        {
                            text += " " + kBullet + " " + tr("not notified");
                        }

                        if (task.isResolved())
                        {
                            text += " [" + tr("RESOLVED") + "]";
                        }

                        taskNode->displayText = text;
                        teamNode->children.append(taskNode);
                    }
                }
            }
        }

        m_teamNodes.append(teamNode);
    }

    // During emergencies, add "Unassigned Tasks" section
    if (emergencyActive)
    {
        const EmergencyResponse& response = EmergencyManager::instance()->response();
        const QHash<FamilyId, FamilyResponseRecord>& records = response.familyRecords();

        // Collect unassigned tasks (not assigned to any team)
        QList<TreeNode*> unassignedNodes;

        for (auto it = records.constBegin(); it != records.constEnd(); ++it)
        {
            const FamilyId& famId = it.key();
            const FamilyResponseRecord& record = it.value();

            for (const ResponseTask& task : record.tasks())
            {
                if (!task.assignedTeamId())
                {
                    TreeNode* taskNode = new TreeNode();
                    taskNode->type = ItemType::TaskRow;
                    taskNode->taskId = task.id();
                    taskNode->familyId = famId;
                    taskNode->taskResolved = task.isResolved();

                    QString text = task.category()
                        + " - " + record.displayName()
                        + " - \"" + task.description() + "\"";

                    if (task.isResolved())
                    {
                        text += " [" + tr("RESOLVED") + "]";
                    }

                    taskNode->displayText = text;
                    unassignedNodes.append(taskNode);
                }
            }
        }

        if (!unassignedNodes.isEmpty())
        {
            TreeNode* unassignedHeader = new TreeNode();
            unassignedHeader->type = ItemType::UnassignedTasksHeader;
            unassignedHeader->displayText = tr("Unassigned Tasks (%1)").arg(unassignedNodes.size());

            for (TreeNode* taskNode : unassignedNodes)
            {
                taskNode->parent = unassignedHeader;
                unassignedHeader->children.append(taskNode);
            }

            m_teamNodes.append(unassignedHeader);
        }
    }

    endResetModel();
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

void TeamsTreeModel::refreshFamilyDisplayText(const FamilyId& familyId)
{
    const Document& doc = DocumentManager::instance()->document();
    const QHash<FamilyId, Family>& families = doc.families();

    if (!families.contains(familyId))
    {
        return;
    }

    const Family& family = families[familyId];

    QSet<PersonId> personIds;
    for (const Person& member : family.members())
    {
        personIds.insert(member.id());
    }

    for (int teamRow = 0; teamRow < m_teamNodes.size(); ++teamRow)
    {
        TreeNode* teamNode = m_teamNodes[teamRow];

        for (int memberRow = 0; memberRow < teamNode->children.size(); ++memberRow)
        {
            TreeNode* memberNode = teamNode->children[memberRow];

            if (memberNode->type == ItemType::TeamMember
                && memberNode->personId
                && personIds.contains(*memberNode->personId))
            {
                std::optional<Person> person = doc.findPersonById(*memberNode->personId);
                if (person)
                {
                    QString displayName = person->displayName() + formatContactSuffix(*person);
                    std::optional<Team> teamOpt = teamNode->teamId
                        ? doc.findTeamById(*teamNode->teamId)
                        : std::nullopt;
                    if (teamOpt && teamOpt->leaderId() && *teamOpt->leaderId() == *memberNode->personId)
                    {
                        displayName += tr(" (leader)");
                    }
                    memberNode->displayText = displayName;

                    QModelIndex teamIndex = createIndex(teamRow, 0, teamNode);
                    QModelIndex memberIndex = index(memberRow, 0, teamIndex);
                    emit dataChanged(memberIndex, memberIndex);
                }
            }
        }
    }
}
