# Tree View Model Migration

## Problem Statement

Several views use `QTreeWidget` with patterns that cause:

1. **Unnecessary rebuilds on selection change** — Views call `rebuildTree()` just to toggle bold styling on the selected item.

2. **Selection state lost during rebuild** — `clear()` fires `currentItemChanged`, which can clear selection state before the rebuild loop reads it.

3. **No keyboard navigation** — Views using `itemClicked` don't respond to arrow key navigation.

4. **Manual state preservation** — Views must manually track and restore selection, expansion, and scroll position across rebuilds.

## Solution

Migrate from `QTreeWidget` to `QTreeView` with custom models. The model/view architecture provides:

- Selection preservation across rebuilds (automatic)
- Expansion state preservation (automatic)
- Scroll position preservation (automatic)
- Keyboard navigation (automatic)
- Standard selection highlighting (no manual bold styling)

## Views to Migrate

| View | Tree Structure | Complexity |
|------|----------------|------------|
| NeedsSubView | Flat list | Simple |
| EmergencyResourceView | 2-level (Resource → Person) | Simple |
| MinisteringView | 4+ levels (District → Companionship → Section → Person → Contact) | Medium |

## Model Pattern

Each view gets a dedicated model class that:

1. Stores pre-processed display data (not raw domain objects)
2. Rebuilds on relevant `DocumentChange` scopes
3. Uses `beginResetModel()` / `endResetModel()` for rebuilds
4. Exposes ID accessors for selection handling

### Minimal Model Interface

```cpp
class MyTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit MyTreeModel(DocumentManager* docMgr, QObject* parent = nullptr);

    // Required QAbstractItemModel overrides
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // View-specific accessors
    QString idAt(const QModelIndex& index) const;
    ItemType typeAt(const QModelIndex& index) const;

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void rebuild();
    bool shouldRebuild(const DocumentChange& change) const;
};
```

### Rebuild Pattern

```cpp
void MyTreeModel::onDocumentChanged(const DocumentChange& change)
{
    if (shouldRebuild(change))
    {
        rebuild();
    }
}

void MyTreeModel::rebuild()
{
    beginResetModel();

    m_data.clear();
    // ... populate m_data from document ...

    endResetModel();
}
```

## NeedsSubView Model

Flat list — simplest case.

```cpp
class NeedsModel : public QAbstractItemModel
{
    struct NeedEntry {
        QString personId;
        QString familyId;
        QString displayText;  // "Name - note"
    };
    QList<NeedEntry> m_entries;

    // Scopes: Full, Family
};
```

- `rowCount(invalid parent)` → `m_entries.size()`
- `rowCount(valid parent)` → 0 (no children)
- `parent(any)` → invalid (flat list)

## EmergencyResourceView Model

2-level tree: Resource → Person

```cpp
class EmergencyResourceModel : public QAbstractItemModel
{
    struct PersonData {
        QString id;
        QString displayName;
    };

    struct ResourceData {
        QString id;
        QString displayText;  // "Name (N)"
        QList<PersonData> people;
    };

    QList<ResourceData> m_resources;
    ResponseArea m_area;

    // Scopes: Full, EmergencyResource, Family
};
```

- Top level: resources (sorted by name)
- Children: people in each resource (sorted by name)
- `internalPointer()` used to distinguish levels

## MinisteringView Model

Multi-level tree with lazy-loaded contact info:

```
District
└── Companionship
    ├── Ministers (section header)
    │   └── Person → Contact details (lazy)
    └── Families/Sisters (section header)
        └── Family/Person → Contact details (lazy)
```

```cpp
class MinisteringModel : public QAbstractItemModel
{
    enum class NodeType { District, Companionship, SectionHeader, Person, ContactDetail };

    // Tree structure using nested structs or node pointers
    // Scopes: Full, MinisteringDistrict, MinisteringGroup, Family
};
```

Options for lazy contact loading:
1. `canFetchMore()` / `fetchMore()` — Qt's lazy loading API
2. Expand signal handler in view — simpler, current pattern

## View Changes

### Before (QTreeWidget)

```cpp
void MyView::setupUi()
{
    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_tree, &QTreeWidget::itemClicked, this, &MyView::onItemClicked);
    connect(m_documentManager, &DocumentManager::documentChanged, this, &MyView::onDocumentChanged);
}

void MyView::onItemClicked(QTreeWidgetItem* item, int column)
{
    // Manual bold toggling, selection tracking...
    rebuildTree();  // Overkill!
    emit highlightChanged();
}

void MyView::onDocumentChanged(const DocumentChange& change)
{
    rebuildTree();
    emit highlightChanged();
}
```

### After (QTreeView + Model)

```cpp
void MyView::setupUi()
{
    m_model = new MyTreeModel(m_documentManager, this);

    m_tree = new QTreeView();
    m_tree->setHeaderHidden(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setModel(m_model);

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MyView::onSelectionChanged);
}

void MyView::onSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous)
    updateButtonStates();
    emit highlightChanged();
}

// No more:
// - Manual bold toggling
// - rebuildTree() in view
// - Selection/expansion tracking
// - Signal blocking
```

## Migration Order

1. **NeedsSubView** — Simplest (flat list), good warmup
2. **EmergencyResourceView** — Simple hierarchy, existing tests
3. **MinisteringView** — Most complex, do last

## Migration Steps (per view)

1. Create model class with rebuild logic extracted from view
2. Create view with QTreeView + model
3. Move selection handling to `currentChanged` signal
4. Update `highlightInfo()` to use model accessors
5. Remove old QTreeWidget code
6. Test: selection, keyboard nav, rebuild preservation, map highlighting

## Design Decisions

- **One model per view** — Views have different data shapes; sharing isn't worth the abstraction cost.
- **Rebuild via reset** — `beginResetModel()` is simpler than surgical updates for small trees. Can optimize later if needed.
- **No base class** — Each model is self-contained. Common patterns but no forced inheritance.
- **Bold styling removed** — Use Qt's standard selection highlighting instead of manual bold.

## Non-Goals

- **Surgical updates** — Still using full rebuilds. Model reset preserves state automatically.
- **Shared base model** — Over-engineering for 3 small models.
- **Lazy loading abstraction** — Handle per-view as needed.
