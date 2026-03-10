# Teams View Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** Replace the Teams placeholder tab with a fully functional view for creating, editing, and managing emergency response teams and their members.

**Architecture:** A 2-level tree (Team → Member) following the `EmergencyAssetView` pattern exactly: `TeamsView` owns a `FilterBar`, toolbar buttons (Add/Edit/Delete), and a `SelectionPreservingTreeView` backed by a `TeamsTreeModel` (BaseTreeModel subclass). All mutations go through existing `TeamCommands`. Member contact details are lazy-loaded on expand. The view implements `FamilyMarkerProvider` so the map highlights families of team members when selected.

**Tech Stack:** Qt 6 / C++17, existing BaseTreeModel/SelectionPreservingTreeView/FilterBar/TeamCommands infrastructure

---

## Task 1: TeamsTreeModel

Create the tree model that powers the team list. This is a 2-level tree: Team → Member, with lazy-loaded contact details as a 3rd level. Follows `EmergencyAssetModel` almost exactly but with teams instead of assets.

**Files:**
- Create: `src/listmodels/TeamsTreeModel.h`
- Create: `src/listmodels/TeamsTreeModel.cpp`
- Modify: `CMakeLists.txt` (add to LISTMODEL_SOURCES and LISTMODEL_HEADERS)

**Step 1: Add ItemType entries**

Add `Team` and `TeamMember` to `src/listmodels/ItemType.h`:

```cpp
// Team types
Team,           // TeamsTreeModel - team group
TeamMember,     // TeamsTreeModel - member of a team
```

Add these after the `Person` entry and before the `// Ministering types` comment.

**Step 2: Write TeamsTreeModel header**

```cpp
#pragma once

#include "BaseTreeModel.h"
#include "DocumentChange.h"

#include <QList>
#include <QString>

class DocumentManager;
class Filter;

/// Model for teams tree (2-level: Team → Member → ContactDetail).
///
/// Level 0: Teams sorted by name, displayed as "Name (N)" where N = member count
/// Level 1: Members in each team, sorted by display name. Leader shown first with "(leader)" suffix.
/// Level 2: Contact details (phone, email, address) - lazy loaded on expand
///
/// Selection key formats:
///   Team         -> {teamId}
///   TeamMember   -> {teamId}:{personId}
///   ContactDetail-> parent's key
class TeamsTreeModel : public BaseTreeModel
{
    Q_OBJECT

public:
    enum Roles
    {
        ItemTypeRole = Qt::UserRole + 1,
        TeamIdRole
    };
    Q_ENUM(Roles)

    explicit TeamsTreeModel(DocumentManager* documentManager,
                            Filter* filter,
                            QObject* parent = nullptr);
    ~TeamsTreeModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;

    // Lazy loading for contact details
    void loadContactDetails(const QModelIndex& index);

    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    SelectionKey selectionKeyAt(const QModelIndex& index) const override;

    /// Returns family associations for the given index.
    FamilyAssociation relatedFamiliesAt(const QModelIndex& index) const;

    // View-specific accessors
    std::optional<TeamId> teamIdAt(const QModelIndex& index) const;
    std::optional<PersonId> personIdAt(const QModelIndex& index) const;

    /// Find the first member node belonging to the given family.
    QModelIndex indexForFamilyId(const FamilyId& familyId) const;

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void rebuild();
    void refreshFamilyDisplayText(const FamilyId& familyId);
    void clearNodes();

    struct TreeNode
    {
        ItemType type = ItemType::Invalid;
        TeamId teamId;
        std::optional<PersonId> personId;
        QString displayText;
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;
        bool contactsLoaded = false;

        ~TreeNode()
        {
            qDeleteAll(children);
        }
    };

    TreeNode* nodeFromIndex(const QModelIndex& index) const;

    QList<TreeNode*> m_teamNodes;  // Top-level team nodes (owned)
    DocumentManager* m_documentManager;
    Filter* m_filter;
};
```

**Step 3: Write TeamsTreeModel implementation**

```cpp
#include "TeamsTreeModel.h"
#include "ContactIcons.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Family.h"
#include "Filter.h"
#include "Person.h"
#include "Team.h"

#include <algorithm>

TeamsTreeModel::TeamsTreeModel(DocumentManager* documentManager,
                               Filter* filter,
                               QObject* parent)
    : BaseTreeModel(parent)
    , m_documentManager(documentManager)
    , m_filter(filter)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &TeamsTreeModel::onDocumentChanged);
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

void TeamsTreeModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = m_documentManager->document();
    QList<Team> teams = doc.teams().values();

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
                QString displayName = person->displayName();
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
                  {
                      bool aIsLeader = leaderId && *leaderId == a.first;
                      bool bIsLeader = leaderId && *leaderId == b.first;
                      if (aIsLeader != bIsLeader)
                      {
                          return aIsLeader;
                      }
                      return a.second.toLower() < b.second.toLower();
                  });

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

        m_teamNodes.append(teamNode);
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
        return true;
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
    case ItemTypeRole:
        return QVariant::fromValue(node->type);
    case TeamIdRole:
        return node->teamId.toString();
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
        return SelectionKey::from(node->teamId);
    case ItemType::TeamMember:
        if (node->personId)
        {
            return SelectionKey::literal(node->teamId.toString() + ":" + node->personId->toString());
        }
        return SelectionKey::from(node->teamId);
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

    const Document& doc = m_documentManager->document();
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return assoc;
    }

    switch (node->type)
    {
    case ItemType::Team:
    {
        std::optional<Team> teamOpt = doc.findTeamById(node->teamId);
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
    const Document& doc = m_documentManager->document();

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

    const Document& doc = m_documentManager->document();
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
    const Document& doc = m_documentManager->document();
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
                    QString displayName = person->displayName();
                    std::optional<Team> teamOpt = doc.findTeamById(teamNode->teamId);
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
```

**Step 4: Update CMakeLists.txt**

Add to `LISTMODEL_SOURCES`:
```
    src/listmodels/TeamsTreeModel.cpp
```

Add to `LISTMODEL_HEADERS`:
```
    src/listmodels/TeamsTreeModel.h
```

**Step 5: Build and verify** — App should compile with new model (unused so far).

**Commit:**
```
feat: add TeamsTreeModel for 2-level team member tree
```

---

## Task 2: TeamsView Widget

Create the view that replaces the placeholder. Follows the `EmergencyAssetView` pattern: FilterBar, toolbar (Add/Edit/Delete), tree view, context menus.

**Files:**
- Create: `src/widgets/TeamsView.h`
- Create: `src/widgets/TeamsView.cpp`
- Modify: `CMakeLists.txt` (add to WIDGET_SOURCES and WIDGET_HEADERS)

**Step 1: Write TeamsView header**

```cpp
#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

#include <optional>

class DocumentManager;
class FilterBar;
class QPushButton;
class QModelIndex;
class SelectionPreservingTreeView;
class TeamsTreeModel;

/// TeamsView displays a 2-level tree of teams and their members.
/// Owns FilterBar (which owns Filter) and TeamsTreeModel internally.
/// Includes a toolbar with Add, Edit, Delete buttons.
class TeamsView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit TeamsView(DocumentManager* documentManager,
                       QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeDoubleClicked(const QModelIndex& index);
    void onTreeExpanded(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);
    void expandTeams();
    void selectMembersFromContextMenu();
    void setLeaderFromContextMenu();
    void clearLeaderFromContextMenu();
    void removeMemberFromContextMenu();

private:
    void updateButtonStates();

    void addTeam();
    void editTeam();
    void deleteTeam();
    void showSelectMembersDialog(const TeamId& teamId);
    void setLeader(const TeamId& teamId, const PersonId& personId);
    void clearLeader(const TeamId& teamId);
    void removeMemberFromTeam(const TeamId& teamId, const PersonId& personId);

    std::optional<TeamId> selectedTeamId() const;

    DocumentManager* m_documentManager;
    FilterBar* m_filterBar;
    TeamsTreeModel* m_model;
    SelectionPreservingTreeView* m_tree;

    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;

    // Context menu state
    std::optional<TeamId> m_contextTeamId;
    std::optional<PersonId> m_contextPersonId;
};
```

**Step 2: Write TeamsView implementation**

```cpp
#include "TeamsView.h"
#include "BaseTreeModel.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Team.h"
#include "TeamsTreeModel.h"
#include "FilterBar.h"
#include "Person.h"
#include "Phone.h"
#include "TeamCommands.h"
#include "SelectionPreservingTreeView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

TeamsView::TeamsView(DocumentManager* documentManager,
                     QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // FilterBar owns Filter
    m_filterBar = new FilterBar(documentManager, this);
    layout->addWidget(m_filterBar);

    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);

    m_addButton = new QPushButton(tr("Add"));
    m_editButton = new QPushButton(tr("Edit"));
    m_deleteButton = new QPushButton(tr("Delete"));

    toolbar->addWidget(m_addButton);
    toolbar->addWidget(m_editButton);
    toolbar->addWidget(m_deleteButton);
    toolbar->addStretch();

    layout->addLayout(toolbar);

    // Create model with filter from FilterBar
    m_model = new TeamsTreeModel(documentManager, m_filterBar->filter(), this);

    // Create tree view with model
    m_tree = new SelectionPreservingTreeView(m_model, this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    m_tree->expandToDepth(0);
    layout->addWidget(m_tree);

    // Connections
    connect(m_addButton, &QPushButton::clicked, this, &TeamsView::addTeam);
    connect(m_editButton, &QPushButton::clicked, this, &TeamsView::editTeam);
    connect(m_deleteButton, &QPushButton::clicked, this, &TeamsView::deleteTeam);

    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &TeamsView::onSelectionChanged);
    connect(m_tree, &QTreeView::doubleClicked,
            this, &TeamsView::onTreeDoubleClicked);
    connect(m_tree, &QTreeView::customContextMenuRequested,
            this, &TeamsView::onContextMenu);

    // Expand top-level items when model is reset
    connect(m_model, &QAbstractItemModel::modelReset, this, &TeamsView::expandTeams);

    // Load contact details when member node is expanded
    connect(m_tree, &QTreeView::expanded,
            this, &TeamsView::onTreeExpanded);

    updateButtonStates();
}

void TeamsView::onSelectionChanged()
{
    updateButtonStates();
    emit highlightChanged();
}

void TeamsView::onTreeDoubleClicked(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    switch (type)
    {
    case ItemType::Team:
        editTeam();
        break;

    case ItemType::TeamMember:
    case ItemType::ContactDetail:
    case ItemType::Invalid:
        break;
    }
}

void TeamsView::onTreeExpanded(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    if (type == ItemType::TeamMember)
    {
        m_model->loadContactDetails(index);
    }
}

void TeamsView::onContextMenu(const QPoint& pos)
{
    QModelIndex index = m_tree->indexAt(pos);

    QMenu menu;

    if (!index.isValid())
    {
        menu.addAction(tr("Add Team..."), this, &TeamsView::addTeam);
    }
    else
    {
        ItemType type = m_model->itemTypeAt(index);

        switch (type)
        {
        case ItemType::Team:
        {
            m_contextTeamId = m_model->teamIdAt(index);
            if (m_contextTeamId)
            {
                menu.addAction(tr("Select Members..."), this, &TeamsView::selectMembersFromContextMenu);
                menu.addSeparator();
                menu.addAction(tr("Rename..."), this, &TeamsView::editTeam);
                menu.addAction(tr("Delete"), this, &TeamsView::deleteTeam);
            }
            break;
        }

        case ItemType::TeamMember:
        {
            // Show contact info (disabled) if available
            std::optional<PersonId> personIdOpt = m_model->personIdAt(index);
            if (!personIdOpt)
            {
                break;
            }
            const Document& doc = m_documentManager->document();
            PersonId personId = *personIdOpt;
            std::optional<Person> personOpt = doc.findPersonById(personId);
            if (personOpt)
            {
                const Phone& phone = personOpt->phone();
                if (!phone.isEmpty())
                {
                    QAction* phoneAction = menu.addAction(phone);
                    phoneAction->setEnabled(false);
                }
                const QString& email = personOpt->email();
                if (!email.isEmpty())
                {
                    QAction* emailAction = menu.addAction(email);
                    emailAction->setEnabled(false);
                }
                if (!phone.isEmpty() || !email.isEmpty())
                {
                    menu.addSeparator();
                }
            }

            m_contextTeamId = m_model->teamIdAt(index);
            m_contextPersonId = personId;
            if (m_contextTeamId)
            {
                // Leader actions
                std::optional<Team> teamOpt = doc.findTeamById(*m_contextTeamId);
                if (teamOpt)
                {
                    bool isLeader = teamOpt->leaderId() && *teamOpt->leaderId() == personId;
                    if (isLeader)
                    {
                        menu.addAction(tr("Clear Leader"), this, &TeamsView::clearLeaderFromContextMenu);
                    }
                    else
                    {
                        menu.addAction(tr("Set as Leader"), this, &TeamsView::setLeaderFromContextMenu);
                    }
                }

                menu.addAction(tr("Remove from Team"), this, &TeamsView::removeMemberFromContextMenu);
            }
            break;
        }

        case ItemType::ContactDetail:
        case ItemType::Invalid:
            break;
        }
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }

    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
}

void TeamsView::expandTeams()
{
    m_tree->expandToDepth(0);
}

void TeamsView::selectMembersFromContextMenu()
{
    std::optional<TeamId> teamId = m_contextTeamId;
    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (teamId)
    {
        showSelectMembersDialog(*teamId);
    }
}

void TeamsView::setLeaderFromContextMenu()
{
    std::optional<TeamId> teamId = m_contextTeamId;
    std::optional<PersonId> personId = m_contextPersonId;
    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (teamId && personId)
    {
        setLeader(*teamId, *personId);
    }
}

void TeamsView::clearLeaderFromContextMenu()
{
    std::optional<TeamId> teamId = m_contextTeamId;
    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (teamId)
    {
        clearLeader(*teamId);
    }
}

void TeamsView::removeMemberFromContextMenu()
{
    std::optional<TeamId> teamId = m_contextTeamId;
    std::optional<PersonId> personId = m_contextPersonId;
    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (teamId && personId)
    {
        removeMemberFromTeam(*teamId, *personId);
    }
}

void TeamsView::updateButtonStates()
{
    bool hasTeamSelected = selectedTeamId().has_value();
    m_editButton->setEnabled(hasTeamSelected);
    m_deleteButton->setEnabled(hasTeamSelected);
}

std::optional<TeamId> TeamsView::selectedTeamId() const
{
    QModelIndex current = m_tree->currentIndex();
    if (!current.isValid())
    {
        return std::nullopt;
    }
    return m_model->teamIdAt(current);
}

HighlightInfo TeamsView::highlightInfo() const
{
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}

QSet<FamilyId> TeamsView::visibleFamilyIds() const
{
    return {};
}

void TeamsView::clearSelection()
{
    m_tree->clearSelection();
}

void TeamsView::selectFamily(const FamilyId& familyId)
{
    QModelIndex idx = m_model->indexForFamilyId(familyId);
    if (idx.isValid())
    {
        m_tree->setCurrentIndex(idx);
        m_tree->scrollTo(idx);
    }
}

void TeamsView::addTeam()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Team"),
                                          tr("Team name:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty())
    {
        Team team = Team::create(name.trimmed());
        m_documentManager->executeCommand(
            std::make_unique<AddTeamCommand>(team));
    }
}

void TeamsView::editTeam()
{
    std::optional<TeamId> teamId = selectedTeamId();
    if (!teamId)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<Team> teamOpt = doc.findTeamById(*teamId);
    if (!teamOpt)
    {
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, tr("Rename Team"),
                                          tr("Team name:"),
                                          QLineEdit::Normal, teamOpt->name(), &ok);
    if (ok && !name.isEmpty() && name.trimmed() != teamOpt->name())
    {
        Team updated = *teamOpt;
        updated.setName(name.trimmed());
        m_documentManager->executeCommand(
            std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
    }
}

void TeamsView::deleteTeam()
{
    std::optional<TeamId> teamId = selectedTeamId();
    if (!teamId)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<Team> teamOpt = doc.findTeamById(*teamId);
    if (!teamOpt)
    {
        return;
    }

    QString message = tr("Delete team \"%1\"?").arg(teamOpt->name());
    if (QMessageBox::question(this, tr("Delete Team"), message) == QMessageBox::Yes)
    {
        m_documentManager->executeCommand(
            std::make_unique<DeleteTeamCommand>(*teamOpt));
    }
}

void TeamsView::showSelectMembersDialog(const TeamId& teamId)
{
    const Document& doc = m_documentManager->document();
    std::optional<Team> teamOpt = doc.findTeamById(teamId);
    if (!teamOpt)
    {
        return;
    }

    QList<PersonId> currentIds = teamOpt->memberIds().values();

    // Use dialog directly to distinguish cancel from empty selection
    WardListDialog dialog(m_documentManager, WardListDialog::PersonMode, this);
    dialog.setSelectionMode(WardListDialog::MultiSelect);
    if (!currentIds.isEmpty())
    {
        dialog.setPreselectedPersonIds(currentIds);
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QList<PersonId> selectedIds = dialog.selectedPersonIds();
    QSet<PersonId> newSet(selectedIds.begin(), selectedIds.end());
    if (newSet == teamOpt->memberIds())
    {
        return;
    }

    Team updated = *teamOpt;
    updated.setMemberIds(newSet);

    // Clear leader if they were removed
    if (updated.leaderId() && !newSet.contains(*updated.leaderId()))
    {
        updated.setLeaderId(std::nullopt);
    }

    m_documentManager->executeCommand(
        std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
}

void TeamsView::setLeader(const TeamId& teamId, const PersonId& personId)
{
    const Document& doc = m_documentManager->document();
    std::optional<Team> teamOpt = doc.findTeamById(teamId);
    if (!teamOpt)
    {
        return;
    }

    Team updated = *teamOpt;
    updated.setLeaderId(personId);
    m_documentManager->executeCommand(
        std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
}

void TeamsView::clearLeader(const TeamId& teamId)
{
    const Document& doc = m_documentManager->document();
    std::optional<Team> teamOpt = doc.findTeamById(teamId);
    if (!teamOpt)
    {
        return;
    }

    Team updated = *teamOpt;
    updated.setLeaderId(std::nullopt);
    m_documentManager->executeCommand(
        std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
}

void TeamsView::removeMemberFromTeam(const TeamId& teamId, const PersonId& personId)
{
    m_documentManager->executeCommand(
        std::make_unique<RemoveTeamMemberCommand>(teamId, personId));
}
```

**Step 3: Update CMakeLists.txt**

Add to `WIDGET_SOURCES`:
```
    src/widgets/TeamsView.cpp
```

Add to `WIDGET_HEADERS`:
```
    src/widgets/TeamsView.h
```

**Step 4: Build and verify** — App should compile with new view (unused so far).

**Commit:**
```
feat: add TeamsView widget with toolbar and context menus
```

---

## Task 3: Wire TeamsView into MainWindow

Replace the `PlaceholderView` with `TeamsView` and connect signals.

**Files:**
- Modify: `src/widgets/MainWindow.h`
- Modify: `src/widgets/MainWindow.cpp`

**Step 1: Update MainWindow.h**

Add forward declaration:
```cpp
class TeamsView;
```

Add member:
```cpp
TeamsView* m_teamsView;
```

**Step 2: Update MainWindow.cpp**

Add include:
```cpp
#include "TeamsView.h"
```

In `setupUi()`, replace:
```cpp
    PlaceholderView* teamsPlaceholder = new PlaceholderView(tr("Teams functionality coming soon"));
    m_sidebarTabs->addPage(teamsPlaceholder, tr("Teams"));
```

With:
```cpp
    m_teamsView = new TeamsView(m_documentManager, this);
    m_sidebarTabs->addPage(m_teamsView, tr("Teams"));
```

In `setupConnections()`, add after the `m_needsView` highlight connection:
```cpp
    connect(m_teamsView, &TeamsView::highlightChanged,
            m_mapWidget, &MapWidget::updateHighlights);
```

**Step 3: Remove PlaceholderView class** — After verifying the app compiles and runs, check if `PlaceholderView` is still used anywhere else. If not, remove the class definition from `MainWindow.cpp`.

**Step 4: Build and verify**

Open the app. Click the "Teams" tab — should show an empty tree with Add/Edit/Delete buttons. Add a team — should appear. Select Members — should open the person picker. Set Leader — member should show "(leader)". Rename, delete — should work. Select a team member — map should highlight their family.

**Commit:**
```
feat: replace Teams placeholder with functional TeamsView
```

---

## Summary of New Files

| File | Purpose |
|------|---------|
| `src/listmodels/TeamsTreeModel.h/cpp` | 2-level tree model (Team → Member → ContactDetail) |
| `src/widgets/TeamsView.h/cpp` | Teams view with toolbar and context menus |

## Modified Files

| File | Change |
|------|--------|
| `src/listmodels/ItemType.h` | Add `Team` and `TeamMember` item types |
| `CMakeLists.txt` | Add new source and header files |
| `src/widgets/MainWindow.h` | Add `TeamsView*` member, forward declaration |
| `src/widgets/MainWindow.cpp` | Replace PlaceholderView with TeamsView, connect highlight signal |
