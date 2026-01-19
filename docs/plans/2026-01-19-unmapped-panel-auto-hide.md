# Unmapped Panel Auto-Hide Design

## Overview

The unmapped panel currently shows/hides based solely on whether unmapped families exist. This design adds context-aware auto-collapse behavior with manual override capability.

## Behavior Summary

| State | Panel Shows |
|-------|-------------|
| Collapsed | Header bar only ("Unknown Location (3)" with expand icon) |
| Expanded | Full panel with markers and labels |

**Auto-expand triggers:**
- Geocoding is in progress
- An unmapped family is selected (from map, panel, or sidebar)

**Auto-collapse triggers:**
- A mapped family is selected
- Selection is cleared (after viewing unmapped)
- Geocoding finishes (if no unmapped family selected)

**Manual override:**
- User can click header to expand/collapse
- Override persists until next auto-expand trigger, then auto-behavior resumes

## State Model

Three pieces of state in MapWidget:

```cpp
bool m_isGeocoding = false;           // Tracked from DocumentManager signals
bool m_unmappedPanelManualOverride = false;  // User manually set state
// m_unmappedPanel->isExpanded() holds visual state
```

**State transitions:**

| Event | Action |
|-------|--------|
| Geocoding starts | Expand panel, clear manual override |
| Unmapped family selected | Expand panel, clear manual override |
| Mapped family selected | If no manual override: collapse panel |
| Selection cleared | If no manual override: collapse panel |
| Geocoding finishes | If no unmapped selected and no manual override: collapse |
| User clicks header | Toggle expanded, set manual override |

## Component Architecture

### MapWidget Changes

**New members:**

```cpp
bool m_isGeocoding = false;
bool m_unmappedPanelManualOverride = false;
```

**New methods:**

```cpp
private:
    void updateUnmappedPanelVisibility();  // Evaluates conditions, updates panel
    bool hasUnmappedSelection() const;     // Checks if highlighted family is unmapped

private slots:
    void onGeocodingStarted();
    void onGeocodingFinished();
    void onUnmappedPanelHeaderClicked();
```

**Constructor additions:**

```cpp
connect(m_docManager, &DocumentManager::geocodingProgressChanged,
        this, &MapWidget::onGeocodingStarted);
connect(m_docManager, &DocumentManager::geocodingFinished,
        this, &MapWidget::onGeocodingFinished);
connect(m_unmappedPanel, &UnmappedPanel::headerClicked,
        this, &MapWidget::onUnmappedPanelHeaderClicked);
```

**Trigger points for updateUnmappedPanelVisibility():**

1. `updateHighlights()` - selection changes
2. `onGeocodingStarted()` - geocoding begins
3. `onGeocodingFinished()` - geocoding ends
4. `onUnmappedPanelHeaderClicked()` - manual toggle

**hasUnmappedSelection() implementation:**

```cpp
bool MapWidget::hasUnmappedSelection() const
{
    if (!m_highlightProvider) return false;
    QSet<QString> highlighted = m_highlightProvider->highlightInfo().allHighlightedIds();
    const Document& doc = m_docManager->document();
    for (const QString& id : highlighted)
    {
        auto family = doc.findFamilyById(id);
        if (family && !family->isMapped()) return true;
    }
    return false;
}
```

**calculateSafeAreaPadding() change:**

Only include unmapped panel in right margin when visible AND expanded:

```cpp
double right = (m_unmappedPanel->isVisible() && m_unmappedPanel->isExpanded())
    ? (width() - m_unmappedPanel->x() + 10)
    : (m_layerButton->width() + 20);
```

### UnmappedPanel Changes

**New members:**

```cpp
private:
    bool m_isExpanded = true;  // Start expanded

signals:
    void headerClicked();
```

**New methods:**

```cpp
void setExpanded(bool expanded);
bool isExpanded() const { return m_isExpanded; }
```

**sizeHint() changes:**

```cpp
QSize UnmappedPanel::sizeHint() const
{
    if (m_layout.isEmpty())
        return QSize(0, 0);

    int width = m_numColumns * COLUMN_WIDTH + H_PADDING * 2;

    if (!m_isExpanded)
        return QSize(width, HEADER_HEIGHT);  // Collapsed: header only

    int height = HEADER_HEIGHT + V_PADDING * 2 + m_rowsPerColumn * ROW_HEIGHT;
    return QSize(width, height);
}
```

**paintEvent() changes:**

- Always draw header with expand/collapse icon ("▸" / "▾") and count
- If collapsed: return after drawing header
- If expanded: continue with marker drawing

**mousePressEvent() changes:**

- Check if click is in header area (y < HEADER_HEIGHT)
- If yes: emit `headerClicked()` signal
- If no and expanded: existing marker click logic

## Visual Design

**Collapsed state:**

```
┌─────────────────────────────────┐
│ ▸ Unknown Location (3)         │
└─────────────────────────────────┘
```

- Height: 24px (HEADER_HEIGHT)
- Width: Matches expanded width (no jumping)
- Expand icon on left
- Count in parentheses
- Entire header clickable

**Expanded state:**

```
┌─────────────────────────────────┐
│ ▾ Unknown Location (3)         │
├─────────────────────────────────┤
│ ● Smith, John                  │
│ ● Jones, Mary                  │
│ ● Brown, David                 │
└─────────────────────────────────┘
```

- Collapse icon on left
- Header click collapses panel
- Marker clicks work as before

**Positioning:**

- Always anchored to bottom-right corner
- Grows upward when expanded

## Files to Modify

1. `src/widgets/MapWidget.h` - Add state members and method declarations
2. `src/widgets/MapWidget.cpp` - Implement visibility logic and signal connections
3. `src/widgets/UnmappedPanel.h` - Add expanded state and headerClicked signal
4. `src/widgets/UnmappedPanel.cpp` - Implement collapsed rendering and click handling
