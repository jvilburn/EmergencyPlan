# FilterBar Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Create a unified FilterBar component that provides consistent search and filtering across all tree views.

**Architecture:** FilterBar is a standalone widget that owns a Filter object and provides UI for all filter types. Views add FilterBar to their layout and pass `filterBar->filter()` to their models. Models connect to `Filter::changed()` and rebuild when filters change.

**Tech Stack:** Qt 6 Widgets, C++17

**Design Doc:** [2026-02-01-tree-view-search-box-design.md](2026-02-01-tree-view-search-box-design.md)

---

## Phase 1: Filter Class Updates

### Task 1: Add AgeCategory enum and update Filter for multi-select

**Files:**
- Modify: `src/listmodels/Filter.h`
- Modify: `src/listmodels/Filter.cpp`

**Step 1: Add AgeCategory enum and change gender/age storage**

In `Filter.h`, add the enum and change member types:

```cpp
// Add after AgeFilter enum (around line 16):
/// Age category for filtering.
enum class AgeCategory
{
    Adult,
    Child
};

// In class Filter private section, change:
// FROM:
//     std::optional<Gender> m_gender;
//     AgeFilter m_ageFilter = AgeFilter::All;
// TO:
    QSet<Gender> m_genders;
    QSet<AgeCategory> m_ageCategories;
```

**Step 2: Update Filter.h getters and setters**

```cpp
// Change getter/setter declarations:
// FROM:
//     std::optional<Gender> gender() const { return m_gender; }
//     void setGender(std::optional<Gender> gender);
//     AgeFilter ageFilter() const { return m_ageFilter; }
//     void setAgeFilter(AgeFilter filter);
// TO:
    QSet<Gender> genders() const { return m_genders; }
    void setGenders(const QSet<Gender>& genders);
    void addGender(Gender gender);
    void removeGender(Gender gender);

    QSet<AgeCategory> ageCategories() const { return m_ageCategories; }
    void setAgeCategories(const QSet<AgeCategory>& categories);
    void addAgeCategory(AgeCategory category);
    void removeAgeCategory(AgeCategory category);
```

**Step 3: Update Filter.cpp setters**

```cpp
void Filter::setGenders(const QSet<Gender>& genders)
{
    if (m_genders != genders)
    {
        m_genders = genders;
        emit changed();
    }
}

void Filter::addGender(Gender gender)
{
    if (!m_genders.contains(gender))
    {
        m_genders.insert(gender);
        emit changed();
    }
}

void Filter::removeGender(Gender gender)
{
    if (m_genders.remove(gender))
    {
        emit changed();
    }
}

void Filter::setAgeCategories(const QSet<AgeCategory>& categories)
{
    if (m_ageCategories != categories)
    {
        m_ageCategories = categories;
        emit changed();
    }
}

void Filter::addAgeCategory(AgeCategory category)
{
    if (!m_ageCategories.contains(category))
    {
        m_ageCategories.insert(category);
        emit changed();
    }
}

void Filter::removeAgeCategory(AgeCategory category)
{
    if (m_ageCategories.remove(category))
    {
        emit changed();
    }
}
```

**Step 4: Update clear() and isEmpty()**

```cpp
// In clear():
    m_genders.clear();
    m_ageCategories.clear();
    // Remove: m_gender = std::nullopt;
    // Remove: m_ageFilter = AgeFilter::All;

// In isEmpty():
    // Change:
    //     && !m_gender.has_value()
    //     && m_ageFilter == AgeFilter::All
    // To:
        && m_genders.isEmpty()
        && m_ageCategories.isEmpty()
```

**Step 5: Update passesPersonCriteria()**

```cpp
bool Filter::passesPersonCriteria(const Person& person) const
{
    // Callings (OR within - existing logic is correct)
    if (!m_callings.isEmpty())
    {
        bool found = false;
        for (const QString& calling : person.callings())
        {
            if (m_callings.contains(calling))
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }

    // Gender (OR within)
    if (!m_genders.isEmpty())
    {
        if (!person.gender().has_value())
        {
            return false;  // Unknown gender doesn't match filter
        }
        if (!m_genders.contains(person.gender().value()))
        {
            return false;
        }
    }

    // Age category (OR within)
    if (!m_ageCategories.isEmpty())
    {
        AgeCategory personCategory = person.isChild() ? AgeCategory::Child : AgeCategory::Adult;
        if (!m_ageCategories.contains(personCategory))
        {
            return false;
        }
    }

    // Specific age (unchanged)
    if (m_specificAge.has_value())
    {
        std::optional<int> personAge = person.age();
        if (!personAge.has_value() || personAge.value() != m_specificAge.value())
        {
            return false;
        }
    }

    return true;
}
```

**Step 6: Build and verify no compile errors**

Run: `build.bat`
Expected: Build succeeds (UI not using new API yet)

**Step 7: Commit**

```bash
git add src/listmodels/Filter.h src/listmodels/Filter.cpp
git commit -m "feat(Filter): change gender/age to multi-select QSet

Change std::optional<Gender> to QSet<Gender> and AgeFilter enum to
QSet<AgeCategory> to support OR logic within same filter type.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 2: Add special needs filtering to Filter

**Files:**
- Modify: `src/listmodels/Filter.h`
- Modify: `src/listmodels/Filter.cpp`

**Step 1: Add special needs fields to Filter.h**

```cpp
// In private section, add:
    QSet<QString> m_specialNeeds;     // Specific need notes to match
    bool m_hasAnySpecialNeed = false; // True = match any special need

// Add getters/setters in public section:
    QSet<QString> specialNeeds() const { return m_specialNeeds; }
    void setSpecialNeeds(const QSet<QString>& needs);
    void addSpecialNeed(const QString& need);
    void removeSpecialNeed(const QString& need);

    bool hasAnySpecialNeed() const { return m_hasAnySpecialNeed; }
    void setHasAnySpecialNeed(bool value);
```

**Step 2: Implement setters in Filter.cpp**

```cpp
void Filter::setSpecialNeeds(const QSet<QString>& needs)
{
    if (m_specialNeeds != needs)
    {
        m_specialNeeds = needs;
        emit changed();
    }
}

void Filter::addSpecialNeed(const QString& need)
{
    if (!m_specialNeeds.contains(need))
    {
        m_specialNeeds.insert(need);
        emit changed();
    }
}

void Filter::removeSpecialNeed(const QString& need)
{
    if (m_specialNeeds.remove(need))
    {
        emit changed();
    }
}

void Filter::setHasAnySpecialNeed(bool value)
{
    if (m_hasAnySpecialNeed != value)
    {
        m_hasAnySpecialNeed = value;
        emit changed();
    }
}
```

**Step 3: Update clear() and isEmpty()**

```cpp
// In clear():
    m_specialNeeds.clear();
    m_hasAnySpecialNeed = false;

// In isEmpty(), add:
        && m_specialNeeds.isEmpty()
        && !m_hasAnySpecialNeed
```

**Step 4: Add special needs check to passesPersonCriteria()**

```cpp
// Add at end of passesPersonCriteria(), before final return:

    // Special needs
    if (m_hasAnySpecialNeed || !m_specialNeeds.isEmpty())
    {
        if (!person.hasSpecialNeed())
        {
            return false;
        }
        // If specific needs specified, must match one (OR within)
        if (!m_specialNeeds.isEmpty())
        {
            if (!m_specialNeeds.contains(person.specialNeedNote()))
            {
                return false;
            }
        }
    }
```

**Step 5: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 6: Commit**

```bash
git add src/listmodels/Filter.h src/listmodels/Filter.cpp
git commit -m "feat(Filter): add special needs filtering

Add m_specialNeeds (QSet<QString>) for specific need matching and
m_hasAnySpecialNeed (bool) for 'any special need' filter.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 3: Add Response Area filtering to Filter

**Files:**
- Modify: `src/listmodels/Filter.h`
- Modify: `src/listmodels/Filter.cpp`

**Step 1: Add includes and fields to Filter.h**

```cpp
// Add include at top:
#include "ResponseArea.h"

// In private section, add:
    QSet<ResponseArea> m_responseAreas;  // Medical, Communications, Recovery
    // Note: m_resourceTypeIds already exists for specific resource IDs
```

**Step 2: Add getters/setters to Filter.h**

```cpp
    QSet<ResponseArea> responseAreas() const { return m_responseAreas; }
    void setResponseAreas(const QSet<ResponseArea>& areas);
    void addResponseArea(ResponseArea area);
    void removeResponseArea(ResponseArea area);
```

**Step 3: Implement setters in Filter.cpp**

```cpp
void Filter::setResponseAreas(const QSet<ResponseArea>& areas)
{
    if (m_responseAreas != areas)
    {
        m_responseAreas = areas;
        emit changed();
    }
}

void Filter::addResponseArea(ResponseArea area)
{
    if (!m_responseAreas.contains(area))
    {
        m_responseAreas.insert(area);
        emit changed();
    }
}

void Filter::removeResponseArea(ResponseArea area)
{
    if (m_responseAreas.remove(area))
    {
        emit changed();
    }
}
```

**Step 4: Update clear() and isEmpty()**

```cpp
// In clear():
    m_responseAreas.clear();

// In isEmpty(), add:
        && m_responseAreas.isEmpty()
```

**Step 5: Implement response area matching helper**

Add to Filter.h private section:
```cpp
    bool hasResponseArea(const Document& document, const QString& personId) const;
```

Add to Filter.cpp:
```cpp
bool Filter::hasResponseArea(const Document& document, const QString& personId) const
{
    QSet<ResponseArea> personAreas = document.personResponseAreas(personId);

    for (ResponseArea area : m_responseAreas)
    {
        if (personAreas.contains(area))
        {
            // If specific resources are also filtered, check those too
            if (!m_resourceTypeIds.isEmpty())
            {
                // Check if person has any of the specific resources in this area
                for (const QString& resourceId : m_resourceTypeIds)
                {
                    std::optional<EmergencyResource> resource = document.findEmergencyResourceById(resourceId);
                    if (resource && resource->responseArea() == area
                        && document.personHasResource(personId, resourceId))
                    {
                        return true;
                    }
                }
            }
            else
            {
                return true;  // Area matches, no specific resource filter
            }
        }
    }
    return false;
}
```

**Step 6: Update passes() methods to use response area**

In `passes(Document, Family)`, update the resources check:
```cpp
    // Change:
    // bool resourcesOk = m_resourceTypeIds.isEmpty()
    //     || hasFamilyLevelResource(document, family.id());
    // To:
    bool resourcesOk = m_responseAreas.isEmpty() && m_resourceTypeIds.isEmpty();
```

In the member loop:
```cpp
        // Change:
        // if (!resourcesOk && hasPersonLevelResource(document, member.id()))
        // To:
        if (!resourcesOk && hasResponseArea(document, member.id()))
        {
            resourcesOk = true;
        }
```

Similar updates for `passes(Document, Person)`.

**Step 7: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 8: Commit**

```bash
git add src/listmodels/Filter.h src/listmodels/Filter.cpp
git commit -m "feat(Filter): add Response Area filtering

Add m_responseAreas (QSet<ResponseArea>) for filtering by Medical,
Communications, Recovery areas. Integrates with existing resourceTypeIds
for filtering by specific resources within an area.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Phase 2: FilterBar Widget

### Task 4: Create FilterChip widget

**Files:**
- Create: `src/widgets/shared/FilterChip.h`
- Create: `src/widgets/shared/FilterChip.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create FilterChip.h**

```cpp
#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

/// A removable filter chip showing filter type and value.
/// Displays as "[Label: Value ×]" with remove button.
class FilterChip : public QFrame
{
    Q_OBJECT

public:
    explicit FilterChip(const QString& label,
                        const QString& value,
                        QWidget* parent = nullptr);

    QString label() const { return m_label; }
    QString value() const { return m_value; }

signals:
    void removeClicked();

private:
    QString m_label;
    QString m_value;
    QLabel* m_textLabel;
    QPushButton* m_removeButton;
};
```

**Step 2: Create FilterChip.cpp**

```cpp
#include "FilterChip.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

FilterChip::FilterChip(const QString& label,
                       const QString& value,
                       QWidget* parent)
    : QFrame(parent)
    , m_label(label)
    , m_value(value)
{
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(
        "FilterChip {"
        "  background: #e0e0e0;"
        "  border: 1px solid #b0b0b0;"
        "  border-radius: 3px;"
        "  padding: 2px 4px;"
        "}"
    );

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 2, 2);
    layout->setSpacing(4);

    // Combine label and value, truncate if too long
    QString displayText = value;
    if (!label.isEmpty())
    {
        displayText = label + ": " + value;
    }
    if (displayText.length() > 30)
    {
        displayText = displayText.left(27) + "...";
    }

    m_textLabel = new QLabel(displayText, this);
    layout->addWidget(m_textLabel);

    m_removeButton = new QPushButton(tr("×"), this);
    m_removeButton->setFixedSize(16, 16);
    m_removeButton->setFlat(true);
    m_removeButton->setStyleSheet(
        "QPushButton { font-weight: bold; color: #606060; }"
        "QPushButton:hover { color: #000000; }"
    );
    layout->addWidget(m_removeButton);

    connect(m_removeButton, &QPushButton::clicked,
            this, &FilterChip::removeClicked);
}
```

**Step 3: Add to CMakeLists.txt**

In WIDGET_SOURCES, add:
```cmake
    src/widgets/shared/FilterChip.cpp
```

In WIDGET_HEADERS, add:
```cmake
    src/widgets/shared/FilterChip.h
```

**Step 4: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/widgets/shared/FilterChip.h src/widgets/shared/FilterChip.cpp CMakeLists.txt
git commit -m "feat: create FilterChip widget

Removable chip showing filter label and value with × button.
Truncates long values to 30 characters.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 5: Create FilterBar widget (basic structure)

**Files:**
- Create: `src/widgets/shared/FilterBar.h`
- Create: `src/widgets/shared/FilterBar.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create FilterBar.h**

```cpp
#pragma once

#include <QWidget>

class DocumentManager;
class Filter;
class FilterChip;
class SearchField;
class QFlowLayout;
class QPushButton;

/// Unified filter UI component with search field, filter chips, and add filter menu.
/// Owns its Filter object internally - access via filter().
class FilterBar : public QWidget
{
    Q_OBJECT

public:
    explicit FilterBar(DocumentManager* docMgr, QWidget* parent = nullptr);

    /// Get the Filter object for connecting to models.
    Filter* filter() const { return m_filter; }

private slots:
    void onSearchTextChanged(const QString& text);
    void onFilterChanged();
    void onAddFilterClicked();
    void onClearAllClicked();

private:
    void rebuildChips();
    void showAddFilterMenu();

    // Add filter menu helpers
    void addTagSubmenu(QMenu* menu);
    void addTeamSubmenu(QMenu* menu);
    void addCallingSubmenu(QMenu* menu);
    void addGenderSubmenu(QMenu* menu);
    void addAgeSubmenu(QMenu* menu);
    void addSpecialNeedsSubmenu(QMenu* menu);
    void addResponseAreaSubmenu(QMenu* menu);

    DocumentManager* m_documentManager;
    Filter* m_filter;
    SearchField* m_searchField;
    QWidget* m_chipsContainer;
    QHBoxLayout* m_chipsLayout;
    QPushButton* m_addFilterButton;
    QPushButton* m_clearAllButton;

    QList<FilterChip*> m_chips;
};
```

**Step 2: Create FilterBar.cpp (basic structure)**

```cpp
#include "FilterBar.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Filter.h"
#include "FilterChip.h"
#include "SearchField.h"
#include "AppStyles.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QPushButton>

FilterBar::FilterBar(DocumentManager* docMgr, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(docMgr)
    , m_filter(new Filter(this))
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // Search field
    m_searchField = new SearchField(this);
    mainLayout->addWidget(m_searchField);

    // Button row: [+ Add Filter] [× Clear All (N)]
    QHBoxLayout* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);

    m_addFilterButton = new QPushButton(tr("+ Add Filter"), this);
    m_addFilterButton->setStyleSheet(AppStyles::beveledButton());
    buttonRow->addWidget(m_addFilterButton);

    m_clearAllButton = new QPushButton(tr("× Clear All"), this);
    m_clearAllButton->setStyleSheet(AppStyles::beveledButton());
    m_clearAllButton->setVisible(false);
    buttonRow->addWidget(m_clearAllButton);

    buttonRow->addStretch();
    mainLayout->addLayout(buttonRow);

    // Chips container
    m_chipsContainer = new QWidget(this);
    m_chipsLayout = new QHBoxLayout(m_chipsContainer);
    m_chipsLayout->setContentsMargins(0, 0, 0, 0);
    m_chipsLayout->setSpacing(4);
    m_chipsLayout->addStretch();
    m_chipsContainer->setVisible(false);
    mainLayout->addWidget(m_chipsContainer);

    // Connections
    connect(m_searchField, &SearchField::searchTextChanged,
            this, &FilterBar::onSearchTextChanged);
    connect(m_filter, &Filter::changed,
            this, &FilterBar::onFilterChanged);
    connect(m_addFilterButton, &QPushButton::clicked,
            this, &FilterBar::onAddFilterClicked);
    connect(m_clearAllButton, &QPushButton::clicked,
            this, &FilterBar::onClearAllClicked);
}

void FilterBar::onSearchTextChanged(const QString& text)
{
    m_filter->setSearchText(text);
}

void FilterBar::onFilterChanged()
{
    rebuildChips();
}

void FilterBar::onAddFilterClicked()
{
    showAddFilterMenu();
}

void FilterBar::onClearAllClicked()
{
    m_filter->clear();
    m_searchField->clear();
}

void FilterBar::rebuildChips()
{
    // Clear existing chips
    for (FilterChip* chip : m_chips)
    {
        chip->deleteLater();
    }
    m_chips.clear();

    // TODO: Add chips for each active filter
    // This will be implemented in the next task

    // Update visibility
    bool hasChips = !m_chips.isEmpty();
    m_chipsContainer->setVisible(hasChips);

    // Update clear all button
    int filterCount = m_chips.size();
    if (!m_filter->searchText().isEmpty())
    {
        filterCount++;
    }
    m_clearAllButton->setVisible(filterCount > 0);
    m_clearAllButton->setText(tr("× Clear All (%1)").arg(filterCount));
}

void FilterBar::showAddFilterMenu()
{
    QMenu menu(this);

    addTagSubmenu(&menu);
    addTeamSubmenu(&menu);
    addCallingSubmenu(&menu);

    menu.addSeparator();

    addGenderSubmenu(&menu);
    addAgeSubmenu(&menu);

    menu.addSeparator();

    addSpecialNeedsSubmenu(&menu);
    addResponseAreaSubmenu(&menu);

    menu.addSeparator();

    // Boolean filters
    QAction* contactAction = menu.addAction(tr("Has Contact Info"));
    contactAction->setCheckable(true);
    contactAction->setChecked(m_filter->onlyWithContact());
    connect(contactAction, &QAction::triggered, this, [this](bool checked) {
        m_filter->setOnlyWithContact(checked);
    });

    QAction* unmappedAction = menu.addAction(tr("Unmapped Only"));
    unmappedAction->setCheckable(true);
    unmappedAction->setChecked(m_filter->mappedFilter() == MappedFilter::Unmapped);
    connect(unmappedAction, &QAction::triggered, this, [this](bool checked) {
        m_filter->setMappedFilter(checked ? MappedFilter::Unmapped : MappedFilter::All);
    });

    menu.exec(m_addFilterButton->mapToGlobal(
        QPoint(0, m_addFilterButton->height())));
}

// Submenu stubs - will be implemented in next task
void FilterBar::addTagSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Tag"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addTeamSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Team"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addCallingSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Calling"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addGenderSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Gender"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addAgeSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Age"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addSpecialNeedsSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Special Needs"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addResponseAreaSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Response Area"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}
```

**Step 3: Add to CMakeLists.txt**

In WIDGET_SOURCES, add:
```cmake
    src/widgets/shared/FilterBar.cpp
```

In WIDGET_HEADERS, add:
```cmake
    src/widgets/shared/FilterBar.h
```

**Step 4: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/widgets/shared/FilterBar.h src/widgets/shared/FilterBar.cpp CMakeLists.txt
git commit -m "feat: create FilterBar widget (basic structure)

FilterBar with search field, add filter button, clear all button.
Menu structure in place with stub submenus. Chip building placeholder.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 6: Implement FilterBar submenus and chip creation

**Files:**
- Modify: `src/widgets/shared/FilterBar.cpp`

**Step 1: Implement addTagSubmenu**

```cpp
void FilterBar::addTagSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Tag"));

    const Document& doc = m_documentManager->document();
    QList<Tag> tags = doc.tags().values();
    std::sort(tags.begin(), tags.end(), [](const Tag& a, const Tag& b) {
        return a.name().toLower() < b.name().toLower();
    });

    if (tags.isEmpty())
    {
        sub->addAction(tr("(No tags)"))->setEnabled(false);
        return;
    }

    for (const Tag& tag : tags)
    {
        QAction* action = sub->addAction(tag.name());
        action->setCheckable(true);
        action->setChecked(m_filter->tagIds().contains(tag.id()));
        connect(action, &QAction::triggered, this, [this, tagId = tag.id()](bool checked) {
            QSet<QString> ids = m_filter->tagIds();
            if (checked)
            {
                ids.insert(tagId);
            }
            else
            {
                ids.remove(tagId);
            }
            m_filter->setTagIds(ids);
        });
    }
}
```

**Step 2: Implement addTeamSubmenu**

```cpp
void FilterBar::addTeamSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Team"));

    const Document& doc = m_documentManager->document();
    QList<Team> teams = doc.teams().values();
    std::sort(teams.begin(), teams.end(), [](const Team& a, const Team& b) {
        return a.name().toLower() < b.name().toLower();
    });

    if (teams.isEmpty())
    {
        sub->addAction(tr("(No teams)"))->setEnabled(false);
        return;
    }

    for (const Team& team : teams)
    {
        QAction* action = sub->addAction(team.name());
        action->setCheckable(true);
        action->setChecked(m_filter->teamIds().contains(team.id()));
        connect(action, &QAction::triggered, this, [this, teamId = team.id()](bool checked) {
            QSet<QString> ids = m_filter->teamIds();
            if (checked)
            {
                ids.insert(teamId);
            }
            else
            {
                ids.remove(teamId);
            }
            m_filter->setTeamIds(ids);
        });
    }
}
```

**Step 3: Implement addCallingSubmenu**

```cpp
void FilterBar::addCallingSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Calling"));

    // Gather all unique callings from document
    const Document& doc = m_documentManager->document();
    QSet<QString> allCallings;
    for (const Family& family : doc.families())
    {
        for (const QString& calling : family.allCallings())
        {
            allCallings.insert(calling);
        }
    }

    QStringList callingList = allCallings.values();
    callingList.sort(Qt::CaseInsensitive);

    if (callingList.isEmpty())
    {
        sub->addAction(tr("(No callings)"))->setEnabled(false);
        return;
    }

    for (const QString& calling : callingList)
    {
        QString displayName = calling;
        if (displayName.length() > 30)
        {
            displayName = displayName.left(27) + "...";
        }

        QAction* action = sub->addAction(displayName);
        action->setCheckable(true);
        action->setChecked(m_filter->callings().contains(calling));
        connect(action, &QAction::triggered, this, [this, calling](bool checked) {
            QSet<QString> callings = m_filter->callings();
            if (checked)
            {
                callings.insert(calling);
            }
            else
            {
                callings.remove(calling);
            }
            m_filter->setCallings(callings);
        });
    }
}
```

**Step 4: Implement addGenderSubmenu**

```cpp
void FilterBar::addGenderSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Gender"));

    QAction* maleAction = sub->addAction(tr("Male"));
    maleAction->setCheckable(true);
    maleAction->setChecked(m_filter->genders().contains(Gender::Male));
    connect(maleAction, &QAction::triggered, this, [this](bool checked) {
        if (checked)
        {
            m_filter->addGender(Gender::Male);
        }
        else
        {
            m_filter->removeGender(Gender::Male);
        }
    });

    QAction* femaleAction = sub->addAction(tr("Female"));
    femaleAction->setCheckable(true);
    femaleAction->setChecked(m_filter->genders().contains(Gender::Female));
    connect(femaleAction, &QAction::triggered, this, [this](bool checked) {
        if (checked)
        {
            m_filter->addGender(Gender::Female);
        }
        else
        {
            m_filter->removeGender(Gender::Female);
        }
    });
}
```

**Step 5: Implement addAgeSubmenu**

```cpp
void FilterBar::addAgeSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Age"));

    QAction* adultAction = sub->addAction(tr("Adult"));
    adultAction->setCheckable(true);
    adultAction->setChecked(m_filter->ageCategories().contains(AgeCategory::Adult));
    connect(adultAction, &QAction::triggered, this, [this](bool checked) {
        if (checked)
        {
            m_filter->addAgeCategory(AgeCategory::Adult);
        }
        else
        {
            m_filter->removeAgeCategory(AgeCategory::Adult);
        }
    });

    QAction* childAction = sub->addAction(tr("Child"));
    childAction->setCheckable(true);
    childAction->setChecked(m_filter->ageCategories().contains(AgeCategory::Child));
    connect(childAction, &QAction::triggered, this, [this](bool checked) {
        if (checked)
        {
            m_filter->addAgeCategory(AgeCategory::Child);
        }
        else
        {
            m_filter->removeAgeCategory(AgeCategory::Child);
        }
    });
}
```

**Step 6: Implement addSpecialNeedsSubmenu**

```cpp
void FilterBar::addSpecialNeedsSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Special Needs"));

    // "(All)" option for any special need
    QAction* allAction = sub->addAction(tr("(All)"));
    allAction->setCheckable(true);
    allAction->setChecked(m_filter->hasAnySpecialNeed());
    connect(allAction, &QAction::triggered, this, [this](bool checked) {
        m_filter->setHasAnySpecialNeed(checked);
    });

    sub->addSeparator();

    // Gather all unique special need notes
    const Document& doc = m_documentManager->document();
    QSet<QString> allNeeds;
    for (const Family& family : doc.families())
    {
        for (const Person& person : family.members())
        {
            if (person.hasSpecialNeed())
            {
                allNeeds.insert(person.specialNeedNote());
            }
        }
    }

    QStringList needsList = allNeeds.values();
    needsList.sort(Qt::CaseInsensitive);

    if (needsList.isEmpty())
    {
        sub->addAction(tr("(No special needs)"))->setEnabled(false);
        return;
    }

    for (const QString& need : needsList)
    {
        QString displayName = need;
        if (displayName.length() > 30)
        {
            displayName = displayName.left(27) + "...";
        }

        QAction* action = sub->addAction(displayName);
        action->setCheckable(true);
        action->setChecked(m_filter->specialNeeds().contains(need));
        connect(action, &QAction::triggered, this, [this, need](bool checked) {
            if (checked)
            {
                m_filter->addSpecialNeed(need);
            }
            else
            {
                m_filter->removeSpecialNeed(need);
            }
        });
    }
}
```

**Step 7: Implement addResponseAreaSubmenu**

```cpp
void FilterBar::addResponseAreaSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Response Area"));

    // Medical submenu
    QMenu* medicalSub = sub->addMenu(tr("Medical"));
    addResponseAreaItems(medicalSub, ResponseArea::Medical);

    // Communications submenu
    QMenu* commSub = sub->addMenu(tr("Communications"));
    addResponseAreaItems(commSub, ResponseArea::Communications);

    // Recovery submenu
    QMenu* recoverySub = sub->addMenu(tr("Recovery"));
    addResponseAreaItems(recoverySub, ResponseArea::Recovery);
}

// Add helper method to FilterBar.h and implement:
void FilterBar::addResponseAreaItems(QMenu* menu, ResponseArea area)
{
    // "(All)" option for any resource in this area
    QAction* allAction = menu->addAction(tr("(All)"));
    allAction->setCheckable(true);
    allAction->setChecked(m_filter->responseAreas().contains(area));
    connect(allAction, &QAction::triggered, this, [this, area](bool checked) {
        if (checked)
        {
            m_filter->addResponseArea(area);
        }
        else
        {
            m_filter->removeResponseArea(area);
        }
    });

    menu->addSeparator();

    // Get resources in this area
    const Document& doc = m_documentManager->document();
    QList<EmergencyResource> resources;
    for (const EmergencyResource& resource : doc.emergencyResources())
    {
        if (resource.responseArea() == area)
        {
            resources.append(resource);
        }
    }

    std::sort(resources.begin(), resources.end(),
              [](const EmergencyResource& a, const EmergencyResource& b) {
        return a.name().toLower() < b.name().toLower();
    });

    for (const EmergencyResource& resource : resources)
    {
        QAction* action = menu->addAction(resource.name());
        action->setCheckable(true);
        action->setChecked(m_filter->resourceTypeIds().contains(resource.id()));
        connect(action, &QAction::triggered, this, [this, resourceId = resource.id()](bool checked) {
            QSet<QString> ids = m_filter->resourceTypeIds();
            if (checked)
            {
                ids.insert(resourceId);
            }
            else
            {
                ids.remove(resourceId);
            }
            m_filter->setResourceTypeIds(ids);
        });
    }
}
```

**Step 8: Implement rebuildChips**

```cpp
void FilterBar::rebuildChips()
{
    // Clear existing chips
    for (FilterChip* chip : m_chips)
    {
        m_chipsLayout->removeWidget(chip);
        chip->deleteLater();
    }
    m_chips.clear();

    const Document& doc = m_documentManager->document();

    // Tags
    for (const QString& tagId : m_filter->tagIds())
    {
        std::optional<Tag> tag = doc.findTagById(tagId);
        if (tag)
        {
            addChip(tr("Tag"), tag->name(), [this, tagId]() {
                QSet<QString> ids = m_filter->tagIds();
                ids.remove(tagId);
                m_filter->setTagIds(ids);
            });
        }
    }

    // Teams
    for (const QString& teamId : m_filter->teamIds())
    {
        std::optional<Team> team = doc.findTeamById(teamId);
        if (team)
        {
            addChip(tr("Team"), team->name(), [this, teamId]() {
                QSet<QString> ids = m_filter->teamIds();
                ids.remove(teamId);
                m_filter->setTeamIds(ids);
            });
        }
    }

    // Callings
    for (const QString& calling : m_filter->callings())
    {
        addChip(tr("Calling"), calling, [this, calling]() {
            QSet<QString> callings = m_filter->callings();
            callings.remove(calling);
            m_filter->setCallings(callings);
        });
    }

    // Gender
    for (const Gender& gender : m_filter->genders())
    {
        addChip("", gender.toString(), [this, gender]() {
            m_filter->removeGender(gender);
        });
    }

    // Age categories
    for (const AgeCategory& cat : m_filter->ageCategories())
    {
        QString label = (cat == AgeCategory::Adult) ? tr("Adult") : tr("Child");
        addChip("", label, [this, cat]() {
            m_filter->removeAgeCategory(cat);
        });
    }

    // Special needs - "any" flag
    if (m_filter->hasAnySpecialNeed())
    {
        addChip(tr("Needs"), tr("(All)"), [this]() {
            m_filter->setHasAnySpecialNeed(false);
        });
    }

    // Special needs - specific
    for (const QString& need : m_filter->specialNeeds())
    {
        addChip(tr("Need"), need, [this, need]() {
            m_filter->removeSpecialNeed(need);
        });
    }

    // Response areas
    for (const ResponseArea& area : m_filter->responseAreas())
    {
        QString areaName;
        switch (area)
        {
            case ResponseArea::Medical: areaName = tr("Medical"); break;
            case ResponseArea::Communications: areaName = tr("Communications"); break;
            case ResponseArea::Recovery: areaName = tr("Recovery"); break;
            default: continue;
        }
        addChip(tr("Area"), areaName, [this, area]() {
            m_filter->removeResponseArea(area);
        });
    }

    // Resource IDs
    for (const QString& resourceId : m_filter->resourceTypeIds())
    {
        std::optional<EmergencyResource> resource = doc.findEmergencyResourceById(resourceId);
        if (resource)
        {
            addChip(tr("Resource"), resource->name(), [this, resourceId]() {
                QSet<QString> ids = m_filter->resourceTypeIds();
                ids.remove(resourceId);
                m_filter->setResourceTypeIds(ids);
            });
        }
    }

    // Has contact info
    if (m_filter->onlyWithContact())
    {
        addChip("", tr("Has Contact"), [this]() {
            m_filter->setOnlyWithContact(false);
        });
    }

    // Unmapped only
    if (m_filter->mappedFilter() == MappedFilter::Unmapped)
    {
        addChip("", tr("Unmapped"), [this]() {
            m_filter->setMappedFilter(MappedFilter::All);
        });
    }

    // Update visibility
    bool hasChips = !m_chips.isEmpty();
    m_chipsContainer->setVisible(hasChips);

    // Update clear all button
    int filterCount = m_chips.size();
    if (!m_filter->searchText().isEmpty())
    {
        filterCount++;
    }
    m_clearAllButton->setVisible(filterCount > 0);
    m_clearAllButton->setText(tr("× Clear All (%1)").arg(filterCount));
}

void FilterBar::addChip(const QString& label, const QString& value,
                        std::function<void()> removeCallback)
{
    FilterChip* chip = new FilterChip(label, value, m_chipsContainer);
    connect(chip, &FilterChip::removeClicked, this, removeCallback);

    // Insert before the stretch
    m_chipsLayout->insertWidget(m_chipsLayout->count() - 1, chip);
    m_chips.append(chip);
}
```

**Step 9: Add includes and helper declaration to FilterBar.h**

```cpp
#include <functional>

// In private section:
    void addResponseAreaItems(QMenu* menu, ResponseArea area);
    void addChip(const QString& label, const QString& value,
                 std::function<void()> removeCallback);
```

**Step 10: Add required includes to FilterBar.cpp**

```cpp
#include "Tag.h"
#include "Team.h"
#include "EmergencyResource.h"
#include "ResponseArea.h"
```

**Step 11: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 12: Commit**

```bash
git add src/widgets/shared/FilterBar.h src/widgets/shared/FilterBar.cpp
git commit -m "feat(FilterBar): implement submenus and chip creation

Complete filter menu with all filter types: tags, teams, callings,
gender, age, special needs, response areas. Chips created for all
active filters with remove functionality.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Phase 3: Integrate FilterBar into Views

### Task 7: Update WardListView to use FilterBar

**Files:**
- Modify: `src/widgets/WardListView.h`
- Modify: `src/widgets/WardListView.cpp`
- Modify: `src/widgets/MainWindow.cpp` (constructor call)

**Step 1: Update WardListView.h**

```cpp
// Remove:
// class Filter;
// class SearchField;

// Add:
class FilterBar;

// Change constructor:
// FROM: WardListView(FamilyTreeModel* model, Filter* filter, DocumentManager* documentManager, ...)
// TO:   WardListView(DocumentManager* documentManager, QWidget* parent = nullptr);

// Remove:
//     SearchField* m_searchField;

// Add:
    FilterBar* m_filterBar;

// Change m_model to owned (not passed in):
    FamilyTreeModel* m_model;
```

**Step 2: Update WardListView.cpp constructor**

```cpp
WardListView::WardListView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    setMinimumWidth(250);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // FilterBar owns Filter
    m_filterBar = new FilterBar(documentManager, this);
    layout->addWidget(m_filterBar);

    // Create model with filter from FilterBar
    m_model = new FamilyTreeModel(documentManager, m_filterBar->filter(), this);

    // Tree view with selection preservation
    m_treeView = new SelectionPreservingTreeView(m_model, this);
    // ... rest of tree setup unchanged ...
    layout->addWidget(m_treeView, 1);

    // Remove: connect(m_searchField, ...)
    // The FilterBar handles search internally

    // ... rest unchanged ...
}
```

**Step 3: Remove onSearchTextChanged slot**

Remove the method implementation and declaration.

**Step 4: Update MainWindow to match new constructor**

Find where WardListView is created and change:
```cpp
// FROM: m_wardListView = new WardListView(m_familyTreeModel, m_filter, m_documentManager, this);
// TO:   m_wardListView = new WardListView(m_documentManager, this);
```

Also remove m_filter and m_familyTreeModel if they were owned by MainWindow (WardListView now owns them).

**Step 5: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 6: Test manually**

1. Run the app
2. Verify search field appears in WardListView
3. Verify typing in search filters families
4. Verify "+ Add Filter" menu appears
5. Verify adding a filter shows a chip
6. Verify removing a chip clears the filter

**Step 7: Commit**

```bash
git add src/widgets/WardListView.h src/widgets/WardListView.cpp src/widgets/MainWindow.cpp
git commit -m "feat(WardListView): switch to FilterBar

WardListView now creates FilterBar and FamilyTreeModel internally.
Removes external Filter dependency.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 8: Update MinisteringTabView to use FilterBar

**Files:**
- Modify: `src/widgets/MinisteringTabView.h`
- Modify: `src/widgets/MinisteringTabView.cpp`
- Modify: `src/listmodels/MinisteringModel.h`
- Modify: `src/listmodels/MinisteringModel.cpp`
- Modify: `src/listmodels/UnassignedMinisteringModel.h`
- Modify: `src/listmodels/UnassignedMinisteringModel.cpp`

**Step 1: Add Filter* parameter to MinisteringModel**

In MinisteringModel.h:
```cpp
// Add forward declaration:
class Filter;

// Change constructor:
explicit MinisteringModel(DocumentManager* documentManager,
                          Filter* filter,
                          MinisteringOrg org,
                          QObject* parent = nullptr);

// Add member:
    Filter* m_filter;
```

In MinisteringModel.cpp:
```cpp
MinisteringModel::MinisteringModel(DocumentManager* documentManager,
                                   Filter* filter,
                                   MinisteringOrg org,
                                   QObject* parent)
    : BaseTreeModel(parent)
    , m_documentManager(documentManager)
    , m_filter(filter)
    , m_org(org)
{
    // Connect to filter changes
    if (m_filter)
    {
        connect(m_filter, &Filter::changed, this, &MinisteringModel::rebuild);
    }
    // ... existing connections ...
}
```

Update rebuild() to filter families:
```cpp
// In the rebuild method, when adding ministered families:
// Check if family passes filter before including
if (m_filter && !m_filter->passes(doc, family))
{
    continue;
}
```

**Step 2: Same changes for UnassignedMinisteringModel**

Follow same pattern as Step 1.

**Step 3: Update MinisteringTabView to use FilterBar**

In MinisteringTabView.h:
```cpp
class FilterBar;

// Add member:
    FilterBar* m_filterBar;
```

In MinisteringTabView.cpp constructor:
```cpp
// Create FilterBar first
m_filterBar = new FilterBar(documentManager, this);

// Pass filter to models
m_mainModel = new MinisteringModel(documentManager, m_filterBar->filter(), org, this);
m_unassignedModel = new UnassignedMinisteringModel(documentManager, m_filterBar->filter(), org, this);

// Add FilterBar to layout (before trees)
layout->addWidget(m_filterBar);
layout->addWidget(m_unassignedView);
layout->addWidget(m_mainView, 1);
```

**Step 4: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 5: Test manually**

1. Run the app
2. Go to Ministering tab
3. Verify FilterBar appears at top
4. Verify filters affect both main and unassigned trees

**Step 6: Commit**

```bash
git add src/widgets/MinisteringTabView.h src/widgets/MinisteringTabView.cpp \
        src/listmodels/MinisteringModel.h src/listmodels/MinisteringModel.cpp \
        src/listmodels/UnassignedMinisteringModel.h src/listmodels/UnassignedMinisteringModel.cpp
git commit -m "feat(MinisteringTabView): add FilterBar with shared filter

Both MinisteringModel and UnassignedMinisteringModel now take Filter*
and respond to filter changes. Single FilterBar shared between both trees.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 9: Update NeedsSubView to use FilterBar

**Files:**
- Modify: `src/widgets/NeedsSubView.h`
- Modify: `src/widgets/NeedsSubView.cpp`
- Modify: `src/listmodels/NeedsModel.h`
- Modify: `src/listmodels/NeedsModel.cpp`

**Step 1: Add Filter* to NeedsModel**

Follow same pattern as MinisteringModel - add Filter* parameter, connect to changes, filter in rebuild.

**Step 2: Update NeedsSubView**

Add FilterBar, pass filter to model.

**Step 3: Build, test, commit**

```bash
git commit -m "feat(NeedsSubView): add FilterBar

NeedsModel now takes Filter* and responds to filter changes.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 10: Update EmergencyResourceView to use FilterBar

**Files:**
- Modify: `src/widgets/EmergencyResourceView.h`
- Modify: `src/widgets/EmergencyResourceView.cpp`
- Modify: `src/listmodels/EmergencyResourceModel.h`
- Modify: `src/listmodels/EmergencyResourceModel.cpp`

Follow same pattern as Task 9.

**Commit:**
```bash
git commit -m "feat(EmergencyResourceView): add FilterBar

EmergencyResourceModel now takes Filter* and responds to filter changes.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Phase 4: WardListDialog Conversion

### Task 11: Create PersonTreeModel

**Files:**
- Create: `src/listmodels/PersonTreeModel.h`
- Create: `src/listmodels/PersonTreeModel.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create PersonTreeModel.h**

```cpp
#pragma once

#include "BaseTreeModel.h"
#include "ItemType.h"

class DocumentManager;
class Filter;

/// Tree model showing persons with expandable contact details.
/// For use in person selection dialogs.
class PersonTreeModel : public BaseTreeModel
{
    Q_OBJECT

public:
    explicit PersonTreeModel(DocumentManager* documentManager,
                             Filter* filter,
                             QObject* parent = nullptr);

    // QAbstractItemModel interface
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString selectionKeyAt(const QModelIndex& index) const override;
    QString familyIdAt(const QModelIndex& index) const override;

    // Person-specific
    QString personIdAt(const QModelIndex& index) const;
    QModelIndex indexForPersonId(const QString& personId) const;

public slots:
    void rebuild();

private:
    void loadContactDetails(const QModelIndex& personIndex);

    DocumentManager* m_documentManager;
    Filter* m_filter;

    struct TreeNode
    {
        ItemType type;
        QString personId;
        QString displayText;
        QList<TreeNode*> children;
        TreeNode* parent = nullptr;

        ~TreeNode() { qDeleteAll(children); }
    };

    TreeNode* m_root = nullptr;
};
```

**Step 2: Create PersonTreeModel.cpp**

Implementation follows similar pattern to FamilyTreeModel but with persons as root items.

**Step 3: Add to CMakeLists.txt**

**Step 4: Build and verify**

**Step 5: Commit**

```bash
git commit -m "feat: create PersonTreeModel

Tree model for person selection with expandable contact details.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 12: Update WardListDialog to use FilterBar + tree views

**Files:**
- Modify: `src/widgets/WardListDialog.h`
- Modify: `src/widgets/WardListDialog.cpp`

**Step 1: Update WardListDialog.h**

```cpp
// Remove:
// class FilterableListWidget;
// class FamilyListModel;
// class PersonListModel;

// Add:
class FilterBar;
class SelectionPreservingTreeView;
class FamilyTreeModel;
class PersonTreeModel;

// Change members:
    FilterBar* m_filterBar = nullptr;
    SelectionPreservingTreeView* m_treeView = nullptr;
    FamilyTreeModel* m_familyModel = nullptr;
    PersonTreeModel* m_personModel = nullptr;
```

**Step 2: Update setupUi()**

Replace FilterableListWidget creation with FilterBar + SelectionPreservingTreeView.

**Step 3: Update selection methods**

Adapt selectedIds(), setSelectedIds() etc. to work with tree models.

**Step 4: Build, test, commit**

```bash
git commit -m "feat(WardListDialog): convert to FilterBar + tree view

Replace FilterableListWidget with FilterBar and SelectionPreservingTreeView.
Uses FamilyTreeModel or PersonTreeModel based on mode.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Phase 5: Cleanup

### Task 13: Remove deprecated components

**Files:**
- Delete: `src/widgets/shared/FilterableListWidget.h`
- Delete: `src/widgets/shared/FilterableListWidget.cpp`
- Delete: `src/listmodels/FamilyListModel.h`
- Delete: `src/listmodels/FamilyListModel.cpp`
- Delete: `src/listmodels/PersonListModel.h`
- Delete: `src/listmodels/PersonListModel.cpp`
- Modify: `CMakeLists.txt` (remove entries)

**Step 1: Verify no remaining usages**

```bash
grep -r "FilterableListWidget\|FamilyListModel\|PersonListModel" src/
```

**Step 2: Delete files**

**Step 3: Update CMakeLists.txt**

**Step 4: Build and verify**

**Step 5: Commit**

```bash
git commit -m "chore: remove deprecated FilterableListWidget and list models

Removed FilterableListWidget, FamilyListModel, PersonListModel.
All replaced by FilterBar + tree views.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Summary

**Phase 1:** Filter class updates (Tasks 1-3)
**Phase 2:** FilterBar widget creation (Tasks 4-6)
**Phase 3:** Integrate into views (Tasks 7-10)
**Phase 4:** WardListDialog conversion (Tasks 11-12)
**Phase 5:** Cleanup (Task 13)

Total: 13 tasks
