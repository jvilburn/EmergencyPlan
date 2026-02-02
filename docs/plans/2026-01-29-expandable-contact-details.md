# Expandable Contact Details Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Enable expanding person nodes in all tree views to show contact details (phone, email, address).

**Architecture:** Add `hasChildren()` override to report person nodes as expandable, and `loadContactDetails()` for lazy-loading contact info when expanded. Wire `expanded` signal in views to trigger loading.

**Tech Stack:** Qt 6 model/view (QAbstractItemModel), existing ContactIcons constants.

---

## Background

Tree views showing people currently can't expand person nodes to show contact details because:
1. `hasChildren()` defaults to `rowCount() > 0`, which returns false for nodes with no children yet
2. Without an expand arrow, users can't trigger the `expanded` signal
3. `loadContactDetails()` never gets called

MinisteringModel and UnassignedMinisteringModel already have `loadContactDetails()` but are missing `hasChildren()`. EmergencyResourceModel and NeedsModel need both.

## Scope

| Model | Has `hasChildren()` | Has `loadContactDetails()` | Action |
|-------|---------------------|---------------------------|--------|
| MinisteringModel | No | Yes | Add `hasChildren()` |
| UnassignedMinisteringModel | No | Yes | Add `hasChildren()` |
| EmergencyResourceModel | No | No | Add both + wire view |
| NeedsModel | No | No | Add both + wire view |
| TeamListModel | N/A | N/A | Placeholder view, skip |

## Reference Pattern

From `MinisteringModel::loadContactDetails()` - creates contact nodes with:
- `ContactIcons::Phone` + phone number
- `ContactIcons::Email` + email
- `ContactIcons::Address` + address

Uses `beginInsertRows()`/`endInsertRows()` for proper model notifications.

---

## Task 1: Add `hasChildren()` to MinisteringModel

**Files:**
- Modify: `src/listmodels/MinisteringModel.h`
- Modify: `src/listmodels/MinisteringModel.cpp`

**Step 1: Add declaration to header**

In `MinisteringModel.h`, add after line 61 (`QVariant data(...)`):

```cpp
    bool hasChildren(const QModelIndex& parent = {}) const override;
```

**Step 2: Add implementation to cpp**

In `MinisteringModel.cpp`, add after `columnCount()` (around line 180):

```cpp
bool MinisteringModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_districtNodes.isEmpty();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (!node)
    {
        return false;
    }

    // Person/family nodes can have contact children (lazy loaded)
    if (node->type == NodeType::Minister
        || node->type == NodeType::MinisteredFamily
        || node->type == NodeType::MinisteredSister)
    {
        return true;
    }

    return !node->children.isEmpty();
}
```

**Step 3: Build and verify**

Run: `build.bat`
Expected: Clean compile

**Step 4: Commit**

```bash
git add src/listmodels/MinisteringModel.h src/listmodels/MinisteringModel.cpp
git commit -m "feat(MinisteringModel): add hasChildren() for expandable person nodes"
```

---

## Task 2: Add `hasChildren()` to UnassignedMinisteringModel

**Files:**
- Modify: `src/listmodels/UnassignedMinisteringModel.h`
- Modify: `src/listmodels/UnassignedMinisteringModel.cpp`

**Step 1: Add declaration to header**

In `UnassignedMinisteringModel.h`, add after line 48 (`QVariant data(...)`):

```cpp
    bool hasChildren(const QModelIndex& parent = {}) const override;
```

**Step 2: Add implementation to cpp**

In `UnassignedMinisteringModel.cpp`, add after `columnCount()` (around line 200):

```cpp
bool UnassignedMinisteringModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return m_headerNode != nullptr;
    }

    TreeNode* node = nodeFromIndex(parent);
    if (!node)
    {
        return false;
    }

    // Family/sister nodes can have contact children (lazy loaded)
    if (node->type == NodeType::MinisteredFamily
        || node->type == NodeType::MinisteredSister)
    {
        return true;
    }

    return !node->children.isEmpty();
}
```

**Step 3: Build and verify**

Run: `build.bat`
Expected: Clean compile

**Step 4: Commit**

```bash
git add src/listmodels/UnassignedMinisteringModel.h src/listmodels/UnassignedMinisteringModel.cpp
git commit -m "feat(UnassignedMinisteringModel): add hasChildren() for expandable person nodes"
```

---

## Task 3: Add `hasChildren()` and `loadContactDetails()` to EmergencyResourceModel

**Files:**
- Modify: `src/listmodels/EmergencyResourceModel.h`
- Modify: `src/listmodels/EmergencyResourceModel.cpp`

**Step 3.1: Add ContactDetail to ItemType enum**

In `EmergencyResourceModel.h`, modify the `ItemType` enum (line 24-29):

```cpp
    enum class ItemType
    {
        Invalid,
        Resource,
        Person,
        ContactDetail
    };
```

**Step 3.2: Add `contactsLoaded` to TreeNode**

In `EmergencyResourceModel.h`, add to TreeNode struct (after line 74):

```cpp
        bool contactsLoaded = false;
```

**Step 3.3: Add method declarations**

In `EmergencyResourceModel.h`, add after `QVariant data(...)` (line 51):

```cpp
    bool hasChildren(const QModelIndex& parent = {}) const override;

    // Lazy loading for contact details
    void loadContactDetails(const QModelIndex& index);
```

**Step 3.4: Add includes to cpp**

In `EmergencyResourceModel.cpp`, add after existing includes:

```cpp
#include "ContactIcons.h"
#include "Family.h"
```

**Step 3.5: Add `hasChildren()` implementation**

In `EmergencyResourceModel.cpp`, add after `columnCount()` (around line 190):

```cpp
bool EmergencyResourceModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_resourceNodes.isEmpty();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (!node)
    {
        return false;
    }

    // Person nodes can have contact children (lazy loaded)
    if (node->type == ItemType::Person)
    {
        return true;
    }

    return !node->children.isEmpty();
}
```

**Step 3.6: Update `parent()` to handle ContactDetail nodes**

The current `parent()` assumes all children have Resource parents. Update to handle 3 levels.

In `EmergencyResourceModel.cpp`, replace `parent()` method (lines 149-167):

```cpp
QModelIndex EmergencyResourceModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // If parent is a resource node (top-level)
    int resourceRow = m_resourceNodes.indexOf(parentNode);
    if (resourceRow >= 0)
    {
        return createIndex(resourceRow, 0, parentNode);
    }

    // Parent is a person node - find its row within the resource
    if (parentNode->parent)
    {
        int personRow = parentNode->parent->children.indexOf(parentNode);
        if (personRow >= 0)
        {
            return createIndex(personRow, 0, parentNode);
        }
    }

    return QModelIndex();
}
```

**Step 3.7: Add `loadContactDetails()` implementation**

In `EmergencyResourceModel.cpp`, add after `resourceIdAt()` (end of file):

```cpp
void EmergencyResourceModel::loadContactDetails(const QModelIndex& index)
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->contactsLoaded)
    {
        return;
    }

    // Only load contacts for person nodes
    if (node->type != ItemType::Person)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<Person> person = doc.findPersonById(node->id);
    if (!person)
    {
        node->contactsLoaded = true;
        return;
    }

    // Check for address availability
    QString familyId = doc.familyIdForPerson(node->id);
    const QHash<QString, Family>& families = doc.families();
    bool hasAddress = false;
    if (!familyId.isEmpty() && families.contains(familyId))
    {
        hasAddress = !families[familyId].address().isEmpty();
    }

    // Count actual items to insert
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

    // Phone
    if (!person->phone().isEmpty())
    {
        TreeNode* phoneNode = new TreeNode();
        phoneNode->type = ItemType::ContactDetail;
        phoneNode->id = node->id;
        phoneNode->displayText = ContactIcons::Phone + person->phone();
        phoneNode->parent = node;
        node->children.append(phoneNode);
    }

    // Alt phone
    if (!person->altPhone().isEmpty())
    {
        TreeNode* altPhoneNode = new TreeNode();
        altPhoneNode->type = ItemType::ContactDetail;
        altPhoneNode->id = node->id;
        altPhoneNode->displayText = ContactIcons::Phone + person->altPhone() + tr(" (alt)");
        altPhoneNode->parent = node;
        node->children.append(altPhoneNode);
    }

    // Email
    if (!person->email().isEmpty())
    {
        TreeNode* emailNode = new TreeNode();
        emailNode->type = ItemType::ContactDetail;
        emailNode->id = node->id;
        emailNode->displayText = ContactIcons::Email + person->email();
        emailNode->parent = node;
        node->children.append(emailNode);
    }

    // Address (from family)
    if (hasAddress)
    {
        const Family& family = families[familyId];
        TreeNode* addrNode = new TreeNode();
        addrNode->type = ItemType::ContactDetail;
        addrNode->id = node->id;
        addrNode->displayText = ContactIcons::Address + family.address().full();
        addrNode->parent = node;
        node->children.append(addrNode);
    }

    endInsertRows();
    node->contactsLoaded = true;
}
```

**Step 3.8: Build and verify**

Run: `build.bat`
Expected: Clean compile

**Step 3.9: Commit**

```bash
git add src/listmodels/EmergencyResourceModel.h src/listmodels/EmergencyResourceModel.cpp
git commit -m "feat(EmergencyResourceModel): add hasChildren() and loadContactDetails()"
```

---

## Task 4: Wire `expanded` signal in EmergencyResourceView

**Files:**
- Modify: `src/widgets/EmergencyResourceView.h`
- Modify: `src/widgets/EmergencyResourceView.cpp`

**Step 4.1: Add slot declaration**

In `EmergencyResourceView.h`, add in private slots section:

```cpp
    void onTreeExpanded(const QModelIndex& index);
```

**Step 4.2: Connect signal in constructor**

In `EmergencyResourceView.cpp`, add after the `customContextMenuRequested` connection (around line 73):

```cpp
    connect(m_tree, &QTreeView::expanded,
            this, &EmergencyResourceView::onTreeExpanded);
```

**Step 4.3: Add slot implementation**

In `EmergencyResourceView.cpp`, add after `onContextMenu()`:

```cpp
void EmergencyResourceView::onTreeExpanded(const QModelIndex& index)
{
    EmergencyResourceModel::ItemType type = m_model->itemTypeAt(index);

    if (type == EmergencyResourceModel::ItemType::Person)
    {
        m_model->loadContactDetails(index);
    }
}
```

**Step 4.4: Build and verify**

Run: `build.bat`
Expected: Clean compile

**Step 4.5: Commit**

```bash
git add src/widgets/EmergencyResourceView.h src/widgets/EmergencyResourceView.cpp
git commit -m "feat(EmergencyResourceView): wire expanded signal to load contact details"
```

---

## Task 5: Convert NeedsModel to tree with contact details

**Files:**
- Modify: `src/listmodels/NeedsModel.h`
- Modify: `src/listmodels/NeedsModel.cpp`

**Step 5.1: Add ItemType enum**

In `NeedsModel.h`, add after the class declaration opens (around line 18):

```cpp
    /// Node types in the tree
    enum class ItemType
    {
        Invalid,
        Person,
        ContactDetail
    };
    Q_ENUM(ItemType)
```

**Step 5.2: Replace NeedEntry with TreeNode**

In `NeedsModel.h`, replace the `NeedEntry` struct (lines 51-57) with:

```cpp
    struct TreeNode
    {
        ItemType type = ItemType::Invalid;
        QString personId;
        QString familyId;
        QString displayText;
        QString sortKey;
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;
        bool contactsLoaded = false;

        ~TreeNode()
        {
            qDeleteAll(children);
        }
    };

    TreeNode* nodeFromIndex(const QModelIndex& index) const;
    void clearNodes();
```

**Step 5.3: Update member variable**

In `NeedsModel.h`, replace `QList<NeedEntry> m_entries;` with:

```cpp
    QList<TreeNode*> m_personNodes;  // Top-level person nodes (owned)
```

**Step 5.4: Add new method declarations**

In `NeedsModel.h`, add after `QVariant data(...)`:

```cpp
    bool hasChildren(const QModelIndex& parent = {}) const override;

    // Lazy loading for contact details
    void loadContactDetails(const QModelIndex& index);
    ItemType itemTypeAt(const QModelIndex& index) const;
```

**Step 5.5: Add include for ContactIcons**

In `NeedsModel.cpp`, add:

```cpp
#include "ContactIcons.h"
```

**Step 5.6: Add destructor and clearNodes**

In `NeedsModel.cpp`, add after constructor:

```cpp
NeedsModel::~NeedsModel()
{
    clearNodes();
}

void NeedsModel::clearNodes()
{
    qDeleteAll(m_personNodes);
    m_personNodes.clear();
}
```

**Step 5.7: Add `nodeFromIndex()` helper**

In `NeedsModel.cpp`:

```cpp
NeedsModel::TreeNode* NeedsModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}
```

**Step 5.8: Update `rebuild()` to use TreeNode**

In `NeedsModel.cpp`, replace `rebuild()`:

```cpp
void NeedsModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = m_documentManager->document();

    // Collect all persons with special needs
    QList<TreeNode*> unsorted;
    for (const Family& family : doc.families())
    {
        for (const Person& person : family.members())
        {
            if (person.hasSpecialNeed())
            {
                TreeNode* node = new TreeNode();
                node->type = ItemType::Person;
                node->personId = person.id();
                node->familyId = family.id();
                node->sortKey = person.displayName().toLower();

                QString text = person.displayName();
                if (!person.specialNeedNote().isEmpty())
                {
                    text += QString(" - %1").arg(person.specialNeedNote());
                }
                node->displayText = text;

                unsorted.append(node);
            }
        }
    }

    // Sort alphabetically by display name
    std::sort(unsorted.begin(), unsorted.end(),
              [](const TreeNode* a, const TreeNode* b) { return a->sortKey < b->sortKey; });

    m_personNodes = unsorted;

    endResetModel();
}
```

**Step 5.9: Update `index()` to use TreeNode**

```cpp
QModelIndex NeedsModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        // Top-level: person rows
        if (row >= 0 && row < m_personNodes.size())
        {
            return createIndex(row, 0, m_personNodes.at(row));
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
```

**Step 5.10: Update `parent()` to handle hierarchy**

```cpp
QModelIndex NeedsModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // Parent is always a person node (top-level)
    int row = m_personNodes.indexOf(parentNode);
    if (row >= 0)
    {
        return createIndex(row, 0, parentNode);
    }

    return QModelIndex();
}
```

**Step 5.11: Update `rowCount()` to handle hierarchy**

```cpp
int NeedsModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return m_personNodes.size();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return node->children.size();
    }

    return 0;
}
```

**Step 5.12: Update `data()` to use TreeNode**

```cpp
QVariant NeedsModel::data(const QModelIndex& index, int role) const
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
    case PersonIdRole:
        return node->personId;
    case FamilyIdRole:
        return node->familyId;
    default:
        return QVariant();
    }
}
```

**Step 5.13: Update accessor methods**

```cpp
QString NeedsModel::personIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }
    return node->personId;
}

QString NeedsModel::familyIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }
    return node->familyId;
}

QModelIndex NeedsModel::indexForPersonId(const QString& personId) const
{
    for (int i = 0; i < m_personNodes.size(); ++i)
    {
        if (m_personNodes.at(i)->personId == personId)
        {
            return createIndex(i, 0, m_personNodes.at(i));
        }
    }
    return QModelIndex();
}

NeedsModel::ItemType NeedsModel::itemTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return ItemType::Invalid;
}
```

**Step 5.14: Add `hasChildren()` implementation**

```cpp
bool NeedsModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_personNodes.isEmpty();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (!node)
    {
        return false;
    }

    // Person nodes can have contact children (lazy loaded)
    if (node->type == ItemType::Person)
    {
        return true;
    }

    return !node->children.isEmpty();
}
```

**Step 5.15: Add `loadContactDetails()` implementation**

```cpp
void NeedsModel::loadContactDetails(const QModelIndex& index)
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->contactsLoaded)
    {
        return;
    }

    if (node->type != ItemType::Person)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<Person> person = doc.findPersonById(node->personId);
    if (!person)
    {
        node->contactsLoaded = true;
        return;
    }

    // Check for address availability
    const QHash<QString, Family>& families = doc.families();
    bool hasAddress = false;
    if (!node->familyId.isEmpty() && families.contains(node->familyId))
    {
        hasAddress = !families[node->familyId].address().isEmpty();
    }

    // Count actual items to insert
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

    // Phone
    if (!person->phone().isEmpty())
    {
        TreeNode* phoneNode = new TreeNode();
        phoneNode->type = ItemType::ContactDetail;
        phoneNode->personId = node->personId;
        phoneNode->familyId = node->familyId;
        phoneNode->displayText = ContactIcons::Phone + person->phone();
        phoneNode->parent = node;
        node->children.append(phoneNode);
    }

    // Alt phone
    if (!person->altPhone().isEmpty())
    {
        TreeNode* altPhoneNode = new TreeNode();
        altPhoneNode->type = ItemType::ContactDetail;
        altPhoneNode->personId = node->personId;
        altPhoneNode->familyId = node->familyId;
        altPhoneNode->displayText = ContactIcons::Phone + person->altPhone() + tr(" (alt)");
        altPhoneNode->parent = node;
        node->children.append(altPhoneNode);
    }

    // Email
    if (!person->email().isEmpty())
    {
        TreeNode* emailNode = new TreeNode();
        emailNode->type = ItemType::ContactDetail;
        emailNode->personId = node->personId;
        emailNode->familyId = node->familyId;
        emailNode->displayText = ContactIcons::Email + person->email();
        emailNode->parent = node;
        node->children.append(emailNode);
    }

    // Address (from family)
    if (hasAddress)
    {
        const Family& family = families[node->familyId];
        TreeNode* addrNode = new TreeNode();
        addrNode->type = ItemType::ContactDetail;
        addrNode->personId = node->personId;
        addrNode->familyId = node->familyId;
        addrNode->displayText = ContactIcons::Address + family.address().full();
        addrNode->parent = node;
        node->children.append(addrNode);
    }

    endInsertRows();
    node->contactsLoaded = true;
}
```

**Step 5.16: Add destructor declaration to header**

In `NeedsModel.h`, add after constructor:

```cpp
    ~NeedsModel() override;
```

**Step 5.17: Build and verify**

Run: `build.bat`
Expected: Clean compile

**Step 5.18: Commit**

```bash
git add src/listmodels/NeedsModel.h src/listmodels/NeedsModel.cpp
git commit -m "feat(NeedsModel): convert to tree with expandable contact details"
```

---

## Task 6: Wire `expanded` signal in NeedsSubView

**Files:**
- Modify: `src/widgets/NeedsSubView.h`
- Modify: `src/widgets/NeedsSubView.cpp`

**Step 6.1: Add slot declaration**

In `NeedsSubView.h`, add in private slots section:

```cpp
    void onTreeExpanded(const QModelIndex& index);
```

**Step 6.2: Remove `setRootIsDecorated(false)`**

In `NeedsSubView.cpp`, in the constructor, remove line 76:

```cpp
    m_tree->setRootIsDecorated(false);  // Flat list, no expand/collapse indicators
```

Replace with:

```cpp
    m_tree->setRootIsDecorated(true);
```

**Step 6.3: Connect signal**

In `NeedsSubView.cpp`, add after `customContextMenuRequested` connection (around line 86):

```cpp
    connect(m_tree, &QTreeView::expanded,
            this, &NeedsSubView::onTreeExpanded);
```

**Step 6.4: Add slot implementation**

In `NeedsSubView.cpp`:

```cpp
void NeedsSubView::onTreeExpanded(const QModelIndex& index)
{
    NeedsModel::ItemType type = m_model->itemTypeAt(index);

    if (type == NeedsModel::ItemType::Person)
    {
        m_model->loadContactDetails(index);
    }
}
```

**Step 6.5: Build and verify**

Run: `build.bat`
Expected: Clean compile

**Step 6.6: Commit**

```bash
git add src/widgets/NeedsSubView.h src/widgets/NeedsSubView.cpp
git commit -m "feat(NeedsSubView): wire expanded signal to load contact details"
```

---

## Manual Testing Checklist

After implementation, verify:

- [ ] MinisteringView: Expand a minister/family/sister node - contact details appear
- [ ] MinisteringView (unassigned): Expand a family/sister node - contact details appear
- [ ] EmergencyResourceView: Expand a person node - contact details appear
- [ ] NeedsSubView: Expand a person node - contact details appear
- [ ] All views: Keyboard navigation works (arrow keys to expand)
- [ ] All views: Selection preserved after model rebuild
- [ ] All views: Map highlighting still works after selection

---

## Future Work (Out of Scope)

- TeamListModel: Placeholder view, will need tree conversion when Teams feature is built
- Extracting shared contact loading to reduce duplication (explicitly deferred per design decision)
