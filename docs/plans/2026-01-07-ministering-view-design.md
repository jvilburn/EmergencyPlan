# Ministering Assignments View - Design (Preparation Mode)

## Overview

A read-only view for reviewing imported ministering assignments with geographic visualization. The view helps identify coverage gaps by showing which families (EQ) or sisters (RS) are assigned to which companionships or districts on a map.

**Mode**: This design is for **Preparation Mode**. Response Mode has a separate ministering view for welfare check tracking - see [Response Mode Design](2026-01-07-response-mode-design.md).

**Purpose**: Review and audit ministering coverage geographically. Not for editing - all data comes from PDF import.

**Layout**: Sidebar (hierarchical list) + shared MapWidget.

---

## Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| EQ/RS handling | Toggle within single view | Less UI surface, easy to compare coverage |
| EQ ministered unit | Families | EQ ministers to families |
| RS ministered unit | Individual sisters | RS ministers to sisters; show sister names, highlight their family on map |
| List structure | Hierarchy only (District > Companionships) | Matches data model, simpler than switchable |
| Map coloring | Implicit from selection | No separate toggle needed |
| Default coloring | None until selection | Neutral starting point |
| Multi-select | Same level only | Can't mix districts and companionships |
| Map architecture | Single shared MapWidget | Zoom/pan consistency across views |
| Unassigned | Separate section in list | EQ: unassigned families. RS: unassigned sisters. |

---

## Sidebar Structure

### EQ View

```
[EQ] [RS]                         <- Segmented control toggle

─────────────────────────────────
Unassigned (3)                    <- Clickable, highlights unassigned families

District North (12 families)      <- Collapsible, multi-selectable
  ■ John Smith, Mary Jones (4)    <- Swatch + names + family count
  ■ Bob Lee, Ann Park (5)
  ■ Tom Brown (3)                 <- Solo minister OK

District South (8 families)
  ■ Jane Doe, Sue Kim (4)
  ■ Mike Chen (4)
─────────────────────────────────
```

### RS View

```
[EQ] [RS]                         <- Segmented control toggle

─────────────────────────────────
Unassigned (5)                    <- Clickable, highlights unassigned sisters

District Relief (15 sisters)      <- Collapsible, multi-selectable
  ■ Sarah White, Lisa Green (4)   <- Swatch + names + sister count
    • Jane Anderson
    • Mary Baker
    • Sue Clark
    • Ann Davis
  ■ Beth Hall, Kim Young (6)

District Charity (12 sisters)
  ■ Pat Jones, Amy Lee (4)
─────────────────────────────────
```

### EQ vs RS Display Differences

| Aspect | EQ View | RS View |
|--------|---------|---------|
| Ministered to | Families | Individual sisters |
| Count shown | Family count | Sister count |
| List items | Family names | Sister names |
| Map highlighting | Family markers | Sister's family markers |
| Unassigned | Families without ministers | Sisters without ministers |

### Companionship Display

Each companionship row shows:
- Color swatch (matches map marker color when selected)
- Minister names (comma-separated)
- Assigned count in parentheses (families for EQ, sisters for RS)

### What's NOT Shown

- Interview dates (not needed for geographic analysis)
- Edit/delete actions (read-only view)

---

## Selection & Coloring Behavior

| Selection State | Map Behavior |
|-----------------|--------------|
| Nothing selected | All markers same default color |
| District(s) selected | Selected districts' families colored by district, others dimmed |
| Companionship(s) selected | Selected companionships' families colored by companionship, others dimmed |
| "Unassigned" selected | Unassigned families highlighted, others dimmed |

### Selection Rules

- Clicking a district clears companionship and unassigned selection
- Clicking a companionship clears district and unassigned selection
- Clicking "Unassigned" clears district and companionship selection
- Ctrl/Cmd+click for multi-select within same level only
- Click again to deselect

### Color Assignment

- Colors auto-generated using HSL with evenly spaced hues
- Each companionship or district gets a distinct color
- Colors regenerate when switching EQ/RS or when document changes
- Colors don't persist to document (session-only)

---

## Map Architecture

### Shared MapWidget with Provider Pattern

Rather than multiple MapWidget instances, use a single shared MapWidget with swappable highlight providers. This ensures zoom/pan position stays consistent when switching sidebar tabs.

```
MainWindow
├── Sidebar (QTabWidget)
│   ├── WardListView        <- implements MapHighlightProvider
│   ├── MinisteringView     <- implements MapHighlightProvider
│   └── ... future views
└── MapWidget (shared)
    └── setHighlightProvider(provider)
```

### MapHighlightProvider Interface

```cpp
class MapHighlightProvider
{
public:
    virtual ~MapHighlightProvider() = default;

    /// Color for a family's map marker (default color if not highlighted)
    virtual QColor familyColor(const QString& familyId) const = 0;

    /// Opacity for a family's map marker (1.0 = full, 0.3 = dimmed)
    virtual qreal familyOpacity(const QString& familyId) const = 0;

    /// Set of family IDs to show (empty = show all)
    virtual QSet<QString> visibleFamilyIds() const = 0;

    /// Optional status icon overlay (empty = no icon)
    /// Response Mode uses this for welfare status icons
    virtual QString familyStatusIcon(const QString& familyId) const { return {}; }
};
```

Each sidebar view implements this interface. When the user switches tabs, MainWindow calls `mapWidget->setHighlightProvider(newView)` and the map re-renders.

**Note:** Response Mode welfare check views use `familyStatusIcon()` to overlay status indicators (checkmark, flag, question mark) on map pins. Preparation Mode views return empty strings (no icons).

---

## MinisteringView Implementation

### Class Structure

```cpp
class MinisteringView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT
public:
    explicit MinisteringView(DocumentManager* docManager, QWidget* parent = nullptr);

    // MapHighlightProvider implementation
    QColor familyColor(const QString& familyId) const override;
    qreal familyOpacity(const QString& familyId) const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();  // Emitted when selection changes

private:
    DocumentManager* m_documentManager;

    // UI
    QButtonGroup* m_orgToggle;      // EQ / RS
    QLabel* m_unassignedLabel;      // "Unassigned (N)"
    QTreeWidget* m_districtTree;

    // State
    bool m_isEQ = true;
    QSet<QString> m_selectedDistrictIds;
    QSet<QString> m_selectedCompanionshipIds;
    bool m_unassignedSelected = false;
    QHash<QString, QColor> m_colorMap;  // ID -> color
};
```

### State Management

- `m_isEQ`: Controls whether viewing EQ or RS ministering data
- `m_selectedDistrictIds`: Currently selected districts (empty if companionships selected)
- `m_selectedCompanionshipIds`: Currently selected companionships (empty if districts selected)
- `m_unassignedSelected`: Whether "Unassigned" section is selected
- `m_colorMap`: Maps district/companionship IDs to their assigned colors

### Color Generation

```cpp
void MinisteringView::regenerateColors()
{
    m_colorMap.clear();

    const auto& districts = m_isEQ
        ? m_documentManager->document().eqDistricts()
        : m_documentManager->document().rsDistricts();
    const auto& groups = m_isEQ
        ? m_documentManager->document().eqGroups()
        : m_documentManager->document().rsGroups();

    // Generate colors for companionships
    int count = groups.size();
    int i = 0;
    for (const auto& group : groups)
    {
        qreal hue = (360.0 * i) / count;
        m_colorMap[group.id()] = QColor::fromHslF(hue / 360.0, 0.7, 0.5);
        ++i;
    }

    // Generate colors for districts (fewer, more distinct)
    count = districts.size();
    i = 0;
    for (const auto& district : districts)
    {
        qreal hue = (360.0 * i) / count;
        m_colorMap[district.id()] = QColor::fromHslF(hue / 360.0, 0.8, 0.45);
        ++i;
    }
}
```

---

## Navigation Integration

### Sidebar Tab Structure

```
Sidebar Tabs:
├── Ward List        <- Existing
├── Ministering      <- This view
├── Emergency        <- Future
└── Resources        <- Future
```

### Tab Switch Handling

```cpp
void MainWindow::onSidebarTabChanged(int index)
{
    QWidget* currentTab = m_sidebar->widget(index);

    if (auto* provider = dynamic_cast<MapHighlightProvider*>(currentTab))
    {
        m_mapWidget->setHighlightProvider(provider);
    }
}
```

---

## Data Flow

```
Document (read-only)
    │
    ├── eqDistricts / rsDistricts
    ├── eqGroups / rsGroups
    └── families (for name lookups)
           │
           ▼
    MinisteringView
    ├── Builds tree from districts/groups
    ├── Resolves minister names from person IDs
    ├── Computes unassigned families
    └── Generates colors
           │
           ▼
    MapHighlightProvider interface
           │
           ▼
    MapWidget renders markers with colors/opacity
```

### Computing Unassigned (EQ: Families, RS: Sisters)

```cpp
// EQ: Returns unassigned family IDs
QSet<QString> MinisteringView::unassignedFamilyIds() const
{
    const auto& families = m_documentManager->document().families();
    const auto& groups = m_documentManager->document().eqGroups();

    QSet<QString> assignedIds;
    for (const auto& group : groups)
    {
        assignedIds.unite(group.familyIds());
    }

    QSet<QString> allIds;
    for (const auto& family : families)
    {
        allIds.insert(family.id());
    }

    return allIds - assignedIds;
}

// RS: Returns unassigned sister (person) IDs
QSet<QString> MinisteringView::unassignedSisterIds() const
{
    const auto& families = m_documentManager->document().families();
    const auto& groups = m_documentManager->document().rsGroups();

    QSet<QString> assignedPersonIds;
    for (const auto& group : groups)
    {
        assignedPersonIds.unite(group.ministeredPersonIds());
    }

    // Collect all adult female person IDs
    QSet<QString> allSisterIds;
    for (const auto& family : families)
    {
        for (const auto& member : family.members())
        {
            if (member.gender() == Gender::Female && member.isParent())
            {
                allSisterIds.insert(member.id());
            }
        }
    }

    return allSisterIds - assignedPersonIds;
}

// Helper: Get family ID for a person (for map highlighting)
QString MinisteringView::familyIdForPerson(const QString& personId) const
{
    return m_documentManager->document().familyIdForPerson(personId);
}
```

When RS view highlights a companionship, it finds each ministered sister's family and highlights that family's map marker.

---

## Future Considerations

Items explicitly out of scope but noted for future:

- **Editing**: Add/edit/delete districts, companionships, assignments
- **Interview tracking**: Show interview dates, highlight overdue
- **Visit history**: Show recent ministering visits per family
- **Print/export**: Generate ministering assignment sheets

## Related Designs

- **[Response Mode Design](2026-01-07-response-mode-design.md)**: Welfare check ministering view shows the same hierarchy but with contact status tracking, progress bars, and minister phone numbers for quick calling. RS sisters show their family's welfare status.

---

## Files to Create

| File | Purpose |
|------|---------|
| `src/widgets/MinisteringView.h` | View header |
| `src/widgets/MinisteringView.cpp` | View implementation |
| `src/widgets/MapHighlightProvider.h` | Provider interface |

### Files to Modify

| File | Change |
|------|--------|
| `src/widgets/MainWindow.h/cpp` | Add sidebar tabs, tab switching logic |
| `src/widgets/MapWidget.h/cpp` | Add highlight provider support |
| `src/widgets/WardListView.h/cpp` | Implement MapHighlightProvider |
| `CMakeLists.txt` | Add new source files |
