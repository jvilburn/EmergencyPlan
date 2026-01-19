# MapHighlightProvider Refactoring Design

**Goal:** Separate data from presentation in map highlighting - providers specify WHAT to highlight, MapWidget decides HOW to render it.

**Problem:** Current interface has providers return colors (presentation concern), which couples data and rendering.

---

## Current Interface

```cpp
class MapHighlightProvider
{
public:
    virtual QColor familyColor(const QString& familyId) const = 0;
    virtual qreal familyOpacity(const QString& familyId) const = 0;
    virtual QSet<QString> visibleFamilyIds() const = 0;
    virtual QString familyStatusIcon(const QString& familyId) const;
};
```

**Issues:**
- Providers decide colors (presentation leak)
- Per-family queries are inefficient (pull model)
- No distinction between "highlighted" and "contact point"

---

## New Interface

```cpp
struct HighlightInfo
{
    QSet<QString> highlightedFamilyIds;    // Families to highlight (glow + ring)
    QSet<QString> contactPointFamilyIds;   // Families to highlight AND show pip (e.g., ministers)
};

class MapHighlightProvider
{
public:
    virtual ~MapHighlightProvider() = default;
    virtual HighlightInfo highlightInfo() const = 0;
};
```

**Benefits:**
- Provider returns semantic data only
- MapWidget owns all rendering decisions
- Push model (provider pushes full state)
- Clean separation of concerns

---

## MapWidget Rendering Logic

| Family State | Z-Order | Shadow | Opacity | Pip |
|--------------|---------|--------|---------|-----|
| Non-highlighted | Behind | No | 30% (dimmed) | No |
| Highlighted | On top | Yes | 100% | No |
| Contact point | On top | Yes | 100% | Yes |

**Notes:**
- Both sets get the highlight effect (glow + ring)
- Contact points additionally show a pip indicator
- Shadow provides visual lift for highlighted items

---

## Usage by View

### Ministering View
- `highlightedFamilyIds` = ministered families
- `contactPointFamilyIds` = ministers' families

### Skills/Equipment/Needs Views
- `highlightedFamilyIds` = families of assigned persons/families
- `contactPointFamilyIds` = {} (empty, no contact point concept)

### Ward List View
- `highlightedFamilyIds` = selected family
- `contactPointFamilyIds` = {} (empty)

---

## Sidebar Correlation

The tree views also show a pip indicator next to contact point names, matching the map. This helps users correlate sidebar selection with map markers.

**Design decision:** Both map and tree independently render "contact point" as pip. This is visual language coordination, not code coupling.

---

## Implementation Status

### Completed: Visual Spike (2026-01-18)

The highlighting visual effect has been proven out in MarkerRenderer:

- **Gaussian blur glow**: 3 stacked blur layers mimicking Flutter's BoxShadow
- **Dark blue ring**: Thin `#1A3366` ring (1.5px) around highlighted markers
- **Smaller markers**: Radius reduced from 13px to 10px
- **Pip indicator**: Blue pip for contact points, z-ordered to draw on top
- **MinisteringView**: Updated to use `isContactPoint()` for minister families

### Remaining: Integration Across App

1. **Refactor MapHighlightProvider interface**
   - Change to semantic `HighlightInfo` struct (sets of IDs only)
   - Remove `familyColor()` - presentation concern
   - Remove `familyOpacity()` - MapWidget computes (dim non-highlighted when highlighting active)
   - Keep `isContactPoint()` or fold into `contactPointFamilyIds` set

2. **Update WardListView**
   - Implement new interface
   - Selected family gets highlight glow
   - No contact points (empty set)

3. **Update other views** (as they're built)
   - Skills/Equipment/Needs views
   - Emergency Tab views

4. **Remove legacy highlighting path**
   - MapWidget has both old `m_highlightedIds` and new provider path
   - Once all views migrated, remove old path
