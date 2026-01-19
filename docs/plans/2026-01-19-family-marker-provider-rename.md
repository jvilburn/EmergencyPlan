# FamilyMarkerProvider Rename

**Goal:** Rename `MapHighlightProvider` to `FamilyMarkerProvider` to better reflect its scope.

## Rationale

The current name `MapHighlightProvider` implies the interface is only about highlighting, but it actually controls:

- **Visibility** - Which families appear on the map (`visibleFamilyIds()`)
- **Highlighting** - Which families get visual emphasis (`highlightInfo()`)
- **Status icons** - Optional overlay icons (`familyStatusIcon()`)

Future additions will include:
- **Badge categories** - Skills/equipment badges (combined icons per category set)
- **Emergency status** - Contacted, okay, needs help during emergency response

The name `FamilyMarkerProvider` makes it clear this interface provides all state needed to render family markers.

## Changes

### Rename

| Before | After |
|--------|-------|
| `MapHighlightProvider` | `FamilyMarkerProvider` |
| `MapHighlightProvider.h` | `FamilyMarkerProvider.h` |

### Files to Update

1. **Interface file:**
   - `src/widgets/MapHighlightProvider.h` → `src/widgets/FamilyMarkerProvider.h`
   - Rename class `MapHighlightProvider` → `FamilyMarkerProvider`

2. **Implementers:**
   - `src/widgets/WardListView.h/.cpp`
   - `src/widgets/MinisteringView.h/.cpp`
   - `src/widgets/WardListDialog.h/.cpp`
   - `src/widgets/EmergencyView.h/.cpp`
   - `src/widgets/SkillsSubView.h/.cpp`
   - `src/widgets/EquipmentSubView.h/.cpp`
   - (future: `NeedsSubView`)

3. **Consumer:**
   - `src/widgets/MapWidget.h/.cpp` - Update member variable and method parameter types

4. **CMakeLists.txt** - Update header filename

## Interface (unchanged for now)

```cpp
#pragma once

#include <QSet>
#include <QString>

struct HighlightInfo
{
    QSet<QString> highlightedFamilyIds;
    QSet<QString> contactPointFamilyIds;

    bool hasHighlighting() const;
    QSet<QString> allHighlightedIds() const;
};

class FamilyMarkerProvider
{
public:
    virtual ~FamilyMarkerProvider() = default;
    virtual HighlightInfo highlightInfo() const = 0;
    virtual QSet<QString> visibleFamilyIds() const = 0;
    virtual QString familyStatusIcon(const QString& familyId) const { return {}; }
};
```

Future additions (badge categories, emergency status) will extend this interface.
