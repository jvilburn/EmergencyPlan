# Ministering View - Expandable Groups Design

## Overview

Enhance the Ministering tab to show ministers and ministered families/sisters when expanding a companionship. Clicking on individual people shows their contact information inline.

## Current State

```
District 1 (5 families)
├─ John Smith, Bob Jones (3)      ← leaf node, no expansion
└─ Alice Green, Mary White (2)
```

## New Tree Structure

### EQ View

```
District 1 (5 families)                    ← expanded by default
├─ John Smith, Bob Jones (3)               ← collapsed by default
│   ├─ Ministers
│   │   ├─ John Smith                      ← click to expand contact info
│   │   │   ├─ 📞 (801) 555-1234
│   │   │   ├─ 📧 john@email.com
│   │   │   └─ 📍 123 Main St, Anytown
│   │   └─ Bob Jones
│   └─ Families
│       ├─ Anderson Family                 ← click to expand contact info
│       │   ├─ 📞 (801) 555-5678
│       │   └─ 📍 456 Oak Ave, Anytown
│       ├─ Brown Family
│       └─ Clark Family
└─ Tom Wilson (2)
```

### RS View

```
District Relief (8 sisters)
├─ Sarah White, Lisa Green (4)
│   ├─ Ministers
│   │   ├─ Sarah White
│   │   └─ Lisa Green
│   └─ Sisters
│       ├─ Jane Anderson
│       ├─ Mary Baker
│       ├─ Sue Clark
│       └─ Ann Davis
└─ Beth Hall (3)
```

## Hierarchy Levels

| Level | Item Type | Expandable | Click Action |
|-------|-----------|------------|--------------|
| 1 | District | Yes (default expanded) | Toggle selection for map highlighting |
| 2 | Companionship | Yes (default collapsed) | Toggle selection for map highlighting |
| 3 | Section header ("Ministers", "Families", "Sisters") | No | None (display only) |
| 4 | Person/Family | Yes | Expand/collapse contact info |
| 5 | Contact detail | No | None (display only) |

## Contact Info Display

When a person or family is expanded, show as child items:

**For a Person (minister or sister):**
- Phone (if available): `📞 (801) 555-1234`
- Alt phone (if available): `📞 (801) 555-9999 (alt)`
- Email (if available): `📧 john@email.com`
- Address (from their family): `📍 123 Main St, Anytown`

**For a Family:**
- Phone (head of household): `📞 (801) 555-1234`
- Address: `📍 123 Main St, Anytown`

Contact items use emoji prefixes for visual distinction. Items with no data are omitted (not shown as empty).

## Selection & Map Highlighting

Selection behavior unchanged from current implementation:

- **Click district** → highlights all families in that district on map
- **Click companionship** → highlights ministers' families + ministered families on map
- **Click person/family (level 4)** → expands/collapses contact info only (no map change)
- **Click contact detail (level 5)** → no action

The existing bold styling for selected items continues to apply at district and companionship levels.

## Implementation Notes

### New ItemTypes

```cpp
enum class ItemType {
    District,
    Companionship,
    SectionHeader,    // "Ministers", "Families", "Sisters"
    Minister,         // Individual minister
    MinisteredFamily, // EQ: family being ministered to
    MinisteredSister, // RS: sister being ministered to
    ContactDetail     // Phone, email, address
};
```

### Data Roles

```cpp
static constexpr int IdRole = Qt::UserRole;           // Person/Family ID
static constexpr int TypeRole = Qt::UserRole + 1;     // ItemType
static constexpr int ContactTypeRole = Qt::UserRole + 2; // For contact details: "phone", "email", "address"
```

### Tree Building Changes

In `rebuildTree()`:
1. Districts remain expanded by default
2. Companionships collapsed by default (remove `setExpanded(true)`)
3. Add "Ministers" section header under each companionship
4. Add minister items under "Ministers"
5. Add "Families" or "Sisters" section header
6. Add ministered family/sister items under that section
7. Contact info NOT pre-populated (added on expand)

### Lazy Contact Info Loading

Contact details are added when a person/family item is expanded (not during initial tree build):

```cpp
void MinisteringView::onTreeItemExpanded(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    if (type == ItemType::Minister ||
        type == ItemType::MinisteredFamily ||
        type == ItemType::MinisteredSister)
    {
        // Only populate if not already done
        if (item->childCount() == 0)
        {
            populateContactInfo(item);
        }
    }
}
```

### Section Headers

Section headers ("Ministers", "Families", "Sisters") are:
- Not selectable
- Not expandable (though they have children)
- Styled distinctly (italic or lighter color)
- Always show their children

## Files to Modify

| File | Changes |
|------|---------|
| `src/widgets/MinisteringView.h` | Add new ItemTypes, contact role, expansion slot |
| `src/widgets/MinisteringView.cpp` | Modify `rebuildTree()`, add `populateContactInfo()`, connect expansion signal |

## Future Considerations

- Click-to-copy phone number or email
- Click phone to initiate call (platform-dependent)
- Show last ministering visit date under family
