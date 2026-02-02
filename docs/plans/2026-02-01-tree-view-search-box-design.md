# Tree View Search Box Design

**Status:** Approved
**Date:** 2026-02-02

## Problem

WardListView has a search box. Other tree views don't. We want consistent search and filtering across all tree views.

## Solution

Create a unified **FilterBar** component that provides search and filtering across all views.

---

## FilterBar Component

A standalone widget that provides unified filter UI.

**Responsibilities:**
- Owns a `Filter` object internally
- Displays search field (always visible)
- Displays active filter chips with remove buttons
- Provides "+ Add Filter" dropdown menu
- "Clear All (N)" button when filters are active
- Exposes `Filter*` for models to connect to

**API:**
```cpp
class FilterBar : public QWidget
{
    Q_OBJECT
public:
    explicit FilterBar(DocumentManager* docMgr, QWidget* parent = nullptr);

    Filter* filter() const;

private:
    Filter* m_filter;
    DocumentManager* m_documentManager;
    SearchField* m_searchField;
    QWidget* m_chipsContainer;
    QPushButton* m_addFilterButton;
    QPushButton* m_clearAllButton;
};
```

**Layout:**
```
┌─────────────────────────────────────────┐
│ 🔍 Search...                        [×] │
│ [+ Add Filter] [× Clear All (5)]        │
│ [Male ×] [Adult ×] [Bishop ×]           │
└─────────────────────────────────────────┘
```

---

## Filter Types

All filters shown everywhere (simpler than context-specific).

| Filter | Menu Structure | Storage |
|--------|----------------|---------|
| Search text | (always visible) | `QString` |
| Tags | Tag → submenu of tags | `QSet<QString>` (IDs) |
| Teams | Team → submenu of teams | `QSet<QString>` (IDs) |
| Gender | Gender → Male / Female | `QSet<Gender>` |
| Age | Age → Adult / Child | `QSet<AgeCategory>` |
| Callings | Calling → submenu (gathered from doc) | `QSet<QString>` |
| Special Needs | Special Needs → (All) / Wheelchair / ... | `QSet<QString>` + bool |
| Response Area | Response Area → Medical/Comm/Recovery → (All) / specific | `QSet<ResponseArea>` + `QSet<QString>` |
| Has Contact Info | ☐ Has Contact Info | `bool` |
| Unmapped Only | ☐ Unmapped Only | `bool` |

**Menu structure:**
```
[+ Add Filter ▾]
    ├── Tag...              → submenu of tags
    ├── Team...             → submenu of teams
    ├── Calling...          → submenu (from document)
    ├── Gender              → Male / Female
    ├── Age                 → Adult / Child
    ├── Special Needs       → (All) / Wheelchair / Oxygen... / "Needs assistance with wal..."
    ├── Response Area       → Medical → (All) / First Aid / CPR Certified / ...
    │                         Communications → (All) / Ham Radio / CERT Trained / ...
    │                         Recovery → (All) / Chainsaw / Generator / ...
    ├── ──────────────
    ├── ☐ Has Contact Info
    └── ☐ Unmapped Only
```

---

## Filter Logic

**Same filter type:** OR
- Gender: Male + Gender: Female = Male OR Female

**Different filter types:** AND
- Gender: Male + Age: Adult = Male AND Adult

**Empty set:** No restriction (passes all)

No special handling for conflicting filters - let results be empty.

---

## Filter Class Changes

```cpp
class Filter : public QObject
{
    // Existing
    QString m_searchText;
    QSet<QString> m_tagIds;
    QSet<QString> m_teamIds;
    bool m_onlyUnmapped;
    bool m_onlyWithContact;

    // New
    QSet<Gender> m_genders;
    QSet<AgeCategory> m_ageCategories;
    QSet<QString> m_callings;
    QSet<QString> m_specialNeeds;
    bool m_hasAnySpecialNeed;  // For "(All)" option
    QSet<ResponseArea> m_responseAreas;
    QSet<QString> m_emergencyResourceIds;

public:
    // Centralized matching logic
    bool matchesPerson(const Person& person) const;
    bool matchesFamily(const Family& family) const;
};
```

---

## View Changes

### WardListView
Replace SearchField with FilterBar:
```cpp
m_filterBar = new FilterBar(docMgr, this);
m_model = new FamilyTreeModel(docMgr, m_filterBar->filter(), this);
```

### WardListDialog
Replace FilterableListWidget with FilterBar + tree view:
- Family mode: `FamilyTreeModel`
- Person mode: New `PersonTreeModel` (persons expand to show contact details)

### MinisteringTabView
One FilterBar shared by both trees:
```cpp
m_filterBar = new FilterBar(documentManager, this);
m_mainModel = new MinisteringModel(documentManager, m_filterBar->filter(), org, this);
m_unassignedModel = new UnassignedMinisteringModel(documentManager, m_filterBar->filter(), org, this);
```

### NeedsSubView & EmergencyResourceView
Add FilterBar, pass filter to model.

---

## Model Changes

All tree models take `Filter*` and connect to `Filter::changed()`.

| Model | Change |
|-------|--------|
| FamilyTreeModel | Support new filter types |
| MinisteringModel | Add Filter* parameter |
| UnassignedMinisteringModel | Add Filter* parameter |
| NeedsModel | Add Filter* parameter |
| EmergencyResourceModel | Add Filter* parameter |

---

## New Files

| File | Purpose |
|------|---------|
| `src/widgets/shared/FilterBar.h/.cpp` | Unified filter UI component |
| `src/widgets/shared/FilterChip.h/.cpp` | Individual removable filter chip |
| `src/listmodels/PersonTreeModel.h/.cpp` | Persons with expandable contact details |

## Files to Modify

| File | Change |
|------|--------|
| `Filter.h/.cpp` | Add new filter types, `matchesPerson()`, `matchesFamily()` |
| `FamilyTreeModel.h/.cpp` | Support new filter types |
| `WardListView.h/.cpp` | Replace SearchField with FilterBar |
| `WardListDialog.h/.cpp` | Replace FilterableListWidget with FilterBar + tree view |
| `MinisteringTabView.h/.cpp` | Add FilterBar, pass filter to models |
| `NeedsSubView.h/.cpp` | Add FilterBar |
| `EmergencyResourceView.h/.cpp` | Add FilterBar |
| `MinisteringModel.h/.cpp` | Add Filter* parameter |
| `UnassignedMinisteringModel.h/.cpp` | Add Filter* parameter |
| `NeedsModel.h/.cpp` | Add Filter* parameter |
| `EmergencyResourceModel.h/.cpp` | Add Filter* parameter |

## Files to Delete

| File | Reason |
|------|--------|
| `FilterableListWidget.h/.cpp` | Replaced by FilterBar + tree |
| `FamilyListModel.h/.cpp` | Replaced by FamilyTreeModel |
| `PersonListModel.h/.cpp` | Replaced by PersonTreeModel |
