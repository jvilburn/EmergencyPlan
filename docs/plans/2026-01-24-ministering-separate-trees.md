# Separate Trees for Ministering Organizations

## Goal

Have separate trees for EQ and RS ministering organizations to:
1. Preserve expansion state when switching tabs
2. Simplify selection handling (no pointer invalidation on tab switch)

## Design

### Data Members

**Remove:**
- `QTreeWidget* m_tree`
- `QTreeWidget* m_unassignedTree`
- `selectedId()`, `selectedType()`, `selectedItem()` accessor functions

**Add:**
```cpp
QTreeWidget* m_eqTree = nullptr;
QTreeWidget* m_rsTree = nullptr;
QTreeWidget* m_eqUnassignedTree = nullptr;
QTreeWidget* m_rsUnassignedTree = nullptr;
```

**Keep (unchanged):**
```cpp
QString m_eqSelectedId;
ItemType m_eqSelectedType = ItemType::District;
QTreeWidgetItem* m_eqSelectedItem = nullptr;

QString m_rsSelectedId;
ItemType m_rsSelectedType = ItemType::District;
QTreeWidgetItem* m_rsSelectedItem = nullptr;
```

### setupUi()

Create all 4 trees with identical configuration. RS trees start hidden.

Use lambdas or helper functions to avoid duplicating tree creation code:
```cpp
auto createTree = [](bool isUnassigned) {
    auto* tree = new QTreeWidget();
    tree->setHeaderHidden(true);
    tree->setRootIsDecorated(true);
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    tree->setIndentation(16);
    if (isUnassigned)
        tree->setFixedHeight(38);
    return tree;
};
```

### Signal Connections

All 4 trees connect to shared slots using `sender()` to determine context:

```cpp
connect(m_eqTree, &QTreeWidget::itemClicked, this, &MinisteringView::onTreeItemClicked);
connect(m_rsTree, &QTreeWidget::itemClicked, this, &MinisteringView::onTreeItemClicked);
connect(m_eqUnassignedTree, &QTreeWidget::itemClicked, this, &MinisteringView::onTreeItemClicked);
connect(m_rsUnassignedTree, &QTreeWidget::itemClicked, this, &MinisteringView::onTreeItemClicked);

connect(m_eqTree, &QTreeWidget::itemExpanded, this, &MinisteringView::onTreeItemExpanded);
connect(m_rsTree, &QTreeWidget::itemExpanded, this, &MinisteringView::onTreeItemExpanded);

connect(m_eqUnassignedTree, &QTreeWidget::itemExpanded, this, &MinisteringView::onUnassignedTreeItemExpanded);
connect(m_rsUnassignedTree, &QTreeWidget::itemExpanded, this, &MinisteringView::onUnassignedTreeItemExpanded);
connect(m_eqUnassignedTree, &QTreeWidget::itemCollapsed, this, &MinisteringView::onUnassignedTreeItemCollapsed);
connect(m_rsUnassignedTree, &QTreeWidget::itemCollapsed, this, &MinisteringView::onUnassignedTreeItemCollapsed);
```

### onOrgToggled() - Tab Switching

Just show/hide, no rebuild:

```cpp
void MinisteringView::onOrgToggled(int id)
{
    m_isEQ = (id == 0);

    m_eqTree->setVisible(m_isEQ);
    m_rsTree->setVisible(!m_isEQ);
    updateUnassignedVisibility();

    emit highlightChanged();
}
```

### Click Handler

Single slot for all trees, uses `sender()`:

```cpp
void MinisteringView::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());

    bool isEQ = (tree == m_eqTree || tree == m_eqUnassignedTree);
    bool isUnassigned = (tree == m_eqUnassignedTree || tree == m_rsUnassignedTree);

    handleTreeItemClicked(item, isEQ, isUnassigned);
}
```

Private helper `handleTreeItemClicked(item, isEQ, isUnassigned)` contains the actual click logic, using local references to the appropriate selection state.

### Expand/Collapse Handlers

Main trees share `onTreeItemExpanded` (populates contact info on expand).

Unassigned trees share handlers that use `sender()`:

```cpp
void MinisteringView::onUnassignedTreeItemExpanded(QTreeWidgetItem* item)
{
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    if (type == ItemType::UnassignedHeader)
        tree->setFixedHeight(200);
    else if (type == ItemType::MinisteredFamily || type == ItemType::MinisteredSister)
        if (item->childCount() == 0)
            populateContactInfo(item);
}

void MinisteringView::onUnassignedTreeItemCollapsed(QTreeWidgetItem* item)
{
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    if (type == ItemType::UnassignedHeader)
        tree->setFixedHeight(38);
}
```

### Rebuild Functions

Shared helpers that derive `isEQ` and selection state from tree pointer:

```cpp
void MinisteringView::rebuildTreeImpl(
    QTreeWidget* tree,
    const QHash<QString, MinisteringDistrict>& districts,
    const QHash<QString, MinisteringGroup>& groups)
{
    bool isEQ = (tree == m_eqTree);
    const QString& selectedId = isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType selectedType = isEQ ? m_eqSelectedType : m_rsSelectedType;

    tree->clear();
    // ... existing rebuildTree logic using local variables
}

void MinisteringView::rebuildUnassignedTreeImpl(QTreeWidget* tree)
{
    bool isEQ = (tree == m_eqUnassignedTree);
    const QString& selectedId = isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType selectedType = isEQ ? m_eqSelectedType : m_rsSelectedType;

    tree->clear();
    // ... existing rebuildUnassignedTree logic using local variables
}
```

### onDocumentChanged()

Rebuilds all 4 trees:

```cpp
void MinisteringView::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change)

    m_eqSelectedItem = nullptr;
    m_rsSelectedItem = nullptr;

    const Document& doc = m_documentManager->document();

    rebuildTreeImpl(m_eqTree, doc.eqDistricts(), doc.eqGroups());
    rebuildTreeImpl(m_rsTree, doc.rsDistricts(), doc.rsGroups());
    rebuildUnassignedTreeImpl(m_eqUnassignedTree);
    rebuildUnassignedTreeImpl(m_rsUnassignedTree);

    updateUnassignedVisibility();
    emit highlightChanged();
}
```

### highlightInfo()

Picks selection state based on `m_isEQ`:

```cpp
HighlightInfo MinisteringView::highlightInfo() const
{
    const QString& selectedId = m_isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType selectedType = m_isEQ ? m_eqSelectedType : m_rsSelectedType;

    if (selectedId.isEmpty())
        return {};

    // ... rest unchanged, uses local selectedId/selectedType
}
```

### Helper: updateUnassignedVisibility()

```cpp
void MinisteringView::updateUnassignedVisibility()
{
    m_eqUnassignedTree->setVisible(m_isEQ && m_eqUnassignedTree->topLevelItemCount() > 0);
    m_rsUnassignedTree->setVisible(!m_isEQ && m_rsUnassignedTree->topLevelItemCount() > 0);
}
```

## Summary

| Slot | Purpose |
|------|---------|
| `onOrgToggled` | Tab switch (show/hide) |
| `onTreeItemClicked` | All 4 trees, uses sender() |
| `onTreeItemExpanded` | Main trees (contact info) |
| `onUnassignedTreeItemExpanded` | Unassigned trees, uses sender() |
| `onUnassignedTreeItemCollapsed` | Unassigned trees, uses sender() |
| `onDocumentChanged` | Rebuild all |

| Helper | Purpose |
|--------|---------|
| `handleTreeItemClicked` | Click logic with selection state |
| `rebuildTreeImpl` | Main tree rebuild |
| `rebuildUnassignedTreeImpl` | Unassigned tree rebuild |
| `updateUnassignedVisibility` | Show/hide unassigned based on content |
