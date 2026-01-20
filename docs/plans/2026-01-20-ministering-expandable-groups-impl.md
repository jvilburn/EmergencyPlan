# Ministering Expandable Groups Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Expand companionships to show ministers and ministered families/sisters with inline contact info.

**Architecture:** Extend the existing QTreeWidget hierarchy with new item types for ministers, ministered families/sisters, section headers, and contact details. Contact info loads lazily on expansion.

**Tech Stack:** Qt 6 QTreeWidget, C++17

**Design Doc:** [2026-01-20-ministering-expandable-groups.md](2026-01-20-ministering-expandable-groups.md)

---

## Task 1: Extend ItemType Enum and Add Data Roles

**Files:**
- Modify: `src/widgets/MinisteringView.h:79-82`

**Step 1: Update the enum and add roles**

Replace the existing ItemType enum and role constants:

```cpp
    // Constants for tree item data roles
    static constexpr int IdRole = Qt::UserRole;
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SecondaryIdRole = Qt::UserRole + 2;  // For contact: person/family ID
    enum class ItemType {
        District,
        Companionship,
        SectionHeader,     // "Ministers", "Families", "Sisters"
        Minister,          // Individual minister person
        MinisteredFamily,  // EQ: family being ministered to
        MinisteredSister,  // RS: sister being ministered to
        ContactDetail      // Phone, email, address line
    };
```

**Step 2: Build and verify no compile errors**

Run: `build.bat`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add src/widgets/MinisteringView.h
git commit -m "feat(ministering): add ItemType enum values for expandable hierarchy"
```

---

## Task 2: Add Expansion Slot and Helper Method Declarations

**Files:**
- Modify: `src/widgets/MinisteringView.h:39-50`

**Step 1: Add new slot and helper declarations**

After `onDocumentChanged` slot, add:

```cpp
    void onTreeItemExpanded(QTreeWidgetItem* item);
```

After `clearSelection()` in private section, add:

```cpp
    void populateContactInfo(QTreeWidgetItem* item);
    void addMinistersSection(QTreeWidgetItem* companionshipItem, const MinisteringGroup& group);
    void addMinisteredSection(QTreeWidgetItem* companionshipItem, const MinisteringGroup& group);
```

**Step 2: Build and verify**

Run: `build.bat`
Expected: Build succeeds (linker errors OK - implementations coming)

**Step 3: Commit**

```bash
git add src/widgets/MinisteringView.h
git commit -m "feat(ministering): declare expansion slot and helper methods"
```

---

## Task 3: Connect Expansion Signal

**Files:**
- Modify: `src/widgets/MinisteringView.cpp:124-126`

**Step 1: Add expansion signal connection**

After the existing `itemClicked` connection (around line 125), add:

```cpp
    connect(m_tree, &QTreeWidget::itemExpanded,
            this, &MinisteringView::onTreeItemExpanded);
```

**Step 2: Build and verify**

Run: `build.bat`
Expected: Linker error for missing `onTreeItemExpanded` (expected)

---

## Task 4: Implement onTreeItemExpanded Slot

**Files:**
- Modify: `src/widgets/MinisteringView.cpp` (add after `onDocumentChanged`, around line 244)

**Step 1: Implement the expansion handler**

```cpp
void MinisteringView::onTreeItemExpanded(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    // Only populate contact info for person/family items
    if (type == ItemType::Minister
        || type == ItemType::MinisteredFamily
        || type == ItemType::MinisteredSister)
    {
        // Only populate if not already done (check for placeholder or empty)
        if (item->childCount() == 0)
        {
            populateContactInfo(item);
        }
    }
}
```

**Step 2: Add stub for populateContactInfo**

```cpp
void MinisteringView::populateContactInfo(QTreeWidgetItem* item)
{
    // TODO: implement in next task
    Q_UNUSED(item)
}
```

**Step 3: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/widgets/MinisteringView.cpp
git commit -m "feat(ministering): add expansion handler with stub"
```

---

## Task 5: Implement populateContactInfo

**Files:**
- Modify: `src/widgets/MinisteringView.cpp`

**Step 1: Replace the stub with full implementation**

```cpp
void MinisteringView::populateContactInfo(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    const Document& doc = m_documentManager->document();

    if (type == ItemType::Minister || type == ItemType::MinisteredSister)
    {
        // Person contact info
        std::optional<Person> person = doc.findPersonById(id);
        if (!person)
        {
            return;
        }

        // Phone
        if (!person->phone().isEmpty())
        {
            auto* phoneItem = new QTreeWidgetItem(item);
            phoneItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x9e ") + person->phone());
            phoneItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
            phoneItem->setFlags(phoneItem->flags() & ~Qt::ItemIsSelectable);
        }

        // Alt phone
        if (!person->altPhone().isEmpty())
        {
            auto* altPhoneItem = new QTreeWidgetItem(item);
            altPhoneItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x9e ") + person->altPhone() + tr(" (alt)"));
            altPhoneItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
            altPhoneItem->setFlags(altPhoneItem->flags() & ~Qt::ItemIsSelectable);
        }

        // Email
        if (!person->email().isEmpty())
        {
            auto* emailItem = new QTreeWidgetItem(item);
            emailItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\xa7 ") + person->email());
            emailItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
            emailItem->setFlags(emailItem->flags() & ~Qt::ItemIsSelectable);
        }

        // Address (from family)
        QString familyId = doc.familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            const auto& families = doc.families();
            if (families.contains(familyId))
            {
                const Family& family = families[familyId];
                if (!family.address().isEmpty())
                {
                    auto* addrItem = new QTreeWidgetItem(item);
                    addrItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x8d ") + family.address().full());
                    addrItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
                    addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsSelectable);
                }
            }
        }
    }
    else if (type == ItemType::MinisteredFamily)
    {
        // Family contact info
        const auto& families = doc.families();
        if (!families.contains(id))
        {
            return;
        }

        const Family& family = families[id];

        // Find head of household for phone
        for (const Person& member : family.members())
        {
            if (member.isParent() && !member.phone().isEmpty())
            {
                auto* phoneItem = new QTreeWidgetItem(item);
                phoneItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x9e ") + member.phone());
                phoneItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
                phoneItem->setFlags(phoneItem->flags() & ~Qt::ItemIsSelectable);
                break;  // Only show first parent's phone
            }
        }

        // Address
        if (!family.address().isEmpty())
        {
            auto* addrItem = new QTreeWidgetItem(item);
            addrItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x8d ") + family.address().full());
            addrItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
            addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsSelectable);
        }
    }
}
```

**Step 2: Add include for Person.h if not present**

Check top of file - if `Person.h` is not included, add it.

**Step 3: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/widgets/MinisteringView.cpp
git commit -m "feat(ministering): implement contact info population"
```

---

## Task 6: Implement addMinistersSection Helper

**Files:**
- Modify: `src/widgets/MinisteringView.cpp`

**Step 1: Add the helper method**

```cpp
void MinisteringView::addMinistersSection(QTreeWidgetItem* companionshipItem, const MinisteringGroup& group)
{
    const Document& doc = m_documentManager->document();

    // Create "Ministers" section header
    auto* ministersHeader = new QTreeWidgetItem(companionshipItem);
    ministersHeader->setText(0, tr("Ministers"));
    ministersHeader->setData(0, TypeRole, static_cast<int>(ItemType::SectionHeader));
    ministersHeader->setFlags(ministersHeader->flags() & ~Qt::ItemIsSelectable);

    // Style header italic
    QFont headerFont = ministersHeader->font(0);
    headerFont.setItalic(true);
    ministersHeader->setFont(0, headerFont);
    ministersHeader->setForeground(0, QColor(100, 100, 100));

    // Add individual ministers
    for (const QString& ministerId : group.ministerIds())
    {
        std::optional<Person> person = doc.findPersonById(ministerId);
        if (!person)
        {
            continue;
        }

        auto* ministerItem = new QTreeWidgetItem(ministersHeader);
        ministerItem->setText(0, person->displayName());
        ministerItem->setData(0, IdRole, ministerId);
        ministerItem->setData(0, TypeRole, static_cast<int>(ItemType::Minister));
        ministerItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    ministersHeader->setExpanded(true);
}
```

**Step 2: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add src/widgets/MinisteringView.cpp
git commit -m "feat(ministering): add ministers section helper"
```

---

## Task 7: Implement addMinisteredSection Helper

**Files:**
- Modify: `src/widgets/MinisteringView.cpp`

**Step 1: Add the helper method**

```cpp
void MinisteringView::addMinisteredSection(QTreeWidgetItem* companionshipItem, const MinisteringGroup& group)
{
    const Document& doc = m_documentManager->document();

    // Create section header - "Families" for EQ, "Sisters" for RS
    auto* ministeredHeader = new QTreeWidgetItem(companionshipItem);
    ministeredHeader->setText(0, m_isEQ ? tr("Families") : tr("Sisters"));
    ministeredHeader->setData(0, TypeRole, static_cast<int>(ItemType::SectionHeader));
    ministeredHeader->setFlags(ministeredHeader->flags() & ~Qt::ItemIsSelectable);

    // Style header italic
    QFont headerFont = ministeredHeader->font(0);
    headerFont.setItalic(true);
    ministeredHeader->setFont(0, headerFont);
    ministeredHeader->setForeground(0, QColor(100, 100, 100));

    if (m_isEQ)
    {
        // EQ: Add families
        const auto& families = doc.families();
        for (const QString& familyId : group.familyIds())
        {
            if (!families.contains(familyId))
            {
                continue;
            }

            const Family& family = families[familyId];
            auto* familyItem = new QTreeWidgetItem(ministeredHeader);
            familyItem->setText(0, family.displayName());
            familyItem->setData(0, IdRole, familyId);
            familyItem->setData(0, TypeRole, static_cast<int>(ItemType::MinisteredFamily));
            familyItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }
    }
    else
    {
        // RS: Add sisters
        for (const QString& personId : group.ministeredPersonIds())
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (!person)
            {
                continue;
            }

            auto* sisterItem = new QTreeWidgetItem(ministeredHeader);
            sisterItem->setText(0, person->displayName());
            sisterItem->setData(0, IdRole, personId);
            sisterItem->setData(0, TypeRole, static_cast<int>(ItemType::MinisteredSister));
            sisterItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }
    }

    ministeredHeader->setExpanded(true);
}
```

**Step 2: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add src/widgets/MinisteringView.cpp
git commit -m "feat(ministering): add ministered section helper"
```

---

## Task 8: Update rebuildTree to Use New Helpers

**Files:**
- Modify: `src/widgets/MinisteringView.cpp:293-343`

**Step 1: Update the companionship building section**

Replace the existing companionship item creation (inside the `for (const QString& groupId : district.groupIds())` loop, after creating `companionshipItem`) with:

```cpp
            auto* companionshipItem = new QTreeWidgetItem(districtItem);
            companionshipItem->setText(0, companionshipText);
            companionshipItem->setData(0, IdRole, group.id());
            companionshipItem->setData(0, TypeRole, static_cast<int>(ItemType::Companionship));

            // Add color swatch as decoration
            if (m_colorMap.contains(group.id()))
            {
                QPixmap swatch(12, 12);
                swatch.fill(m_colorMap[group.id()]);
                companionshipItem->setIcon(0, QIcon(swatch));
            }

            // Bold if selected
            if (m_selectedCompanionshipIds.contains(group.id()))
            {
                QFont font = companionshipItem->font(0);
                font.setBold(true);
                companionshipItem->setFont(0, font);
            }

            // Add ministers and ministered sections (new)
            addMinistersSection(companionshipItem, group);
            addMinisteredSection(companionshipItem, group);
```

**Step 2: Remove auto-expand of districts, keep companionships collapsed**

At the end of the district loop, change:

```cpp
        m_tree->addTopLevelItem(districtItem);
        districtItem->setExpanded(true);
```

The `setExpanded(true)` keeps districts expanded. Companionships are collapsed by default (no `setExpanded` call for them).

**Step 3: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 4: Test manually**

1. Launch the app
2. Go to Ministering tab
3. Verify districts are expanded, companionships are collapsed
4. Expand a companionship - should see "Ministers" and "Families"/"Sisters" sections
5. Expand a minister or family - should see contact info with emoji icons

**Step 5: Commit**

```bash
git add src/widgets/MinisteringView.cpp
git commit -m "feat(ministering): integrate section helpers into tree building"
```

---

## Task 9: Update Click Handler to Ignore New Item Types

**Files:**
- Modify: `src/widgets/MinisteringView.cpp:181-234` (onTreeItemClicked)

**Step 1: Update the click handler**

At the start of `onTreeItemClicked`, add early return for non-selectable types:

```cpp
void MinisteringView::onTreeItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    // Section headers and contact details are not interactive for selection
    if (type == ItemType::SectionHeader || type == ItemType::ContactDetail)
    {
        return;
    }

    // Ministers, ministered families/sisters just expand/collapse - no map selection change
    if (type == ItemType::Minister
        || type == ItemType::MinisteredFamily
        || type == ItemType::MinisteredSister)
    {
        // Toggle expansion
        item->setExpanded(!item->isExpanded());
        return;
    }

    QString itemId = item->data(0, IdRole).toString();

    if (type == ItemType::District)
    {
        // ... rest of existing code
```

**Step 2: Build and verify**

Run: `build.bat`
Expected: Build succeeds

**Step 3: Test manually**

1. Click on a companionship - should toggle map highlighting
2. Click on a minister - should expand/collapse to show contact info, NO map change
3. Click on a ministered family - should expand/collapse, NO map change
4. Click on contact detail item - nothing happens

**Step 4: Commit**

```bash
git add src/widgets/MinisteringView.cpp
git commit -m "feat(ministering): update click handler for new item types"
```

---

## Task 10: Final Testing and Cleanup

**Step 1: Full manual test**

Test matrix:
- [ ] EQ view: districts expanded, companionships collapsed
- [ ] EQ view: expand companionship shows Ministers + Families
- [ ] EQ view: expand minister shows phone/email/address
- [ ] EQ view: expand family shows phone/address
- [ ] EQ view: click companionship toggles map highlighting
- [ ] EQ view: click minister/family only expands, no map change
- [ ] RS view: same tests with Sisters instead of Families
- [ ] Switch EQ/RS: tree rebuilds correctly
- [ ] Unassigned selection still works

**Step 2: Check for any missing includes**

Ensure these are at top of MinisteringView.cpp:
- `#include "Person.h"`
- `#include "Family.h"`

**Step 3: Final commit if any cleanup needed**

```bash
git add -A
git commit -m "feat(ministering): complete expandable groups implementation"
```

---

## Summary

| Task | Description |
|------|-------------|
| 1 | Extend ItemType enum and add data roles |
| 2 | Add slot and helper declarations to header |
| 3 | Connect expansion signal |
| 4 | Implement onTreeItemExpanded slot |
| 5 | Implement populateContactInfo |
| 6 | Implement addMinistersSection |
| 7 | Implement addMinisteredSection |
| 8 | Update rebuildTree to use helpers |
| 9 | Update click handler for new types |
| 10 | Final testing and cleanup |
