# Map Animation UI Padding Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make map animations account for UI overlays so target markers don't land under buttons or panels.

**Architecture:** Add a helper method `calculateSafeAreaPadding()` that returns a `QMarginsF` representing the screen areas occupied by UI overlays. Use this padding when calculating the zoom level needed to show both original view and target, so the target lands in the visible/unobstructed area.

**Tech Stack:** Qt 6, C++17

---

## Current UI Overlay Layout

```
+------------------+------------------+
| [+] Zoom In      |    [Layer Btn]   |  <- Top row
| [-] Zoom Out     |                  |
| [⟲] Recenter     |                  |
|                  |                  |
|                  |                  |
|                  |  +------------+  |
|                  |  | Unmapped   |  |
|                  |  | Panel      |  |
| [Attribution]    |  | (dynamic)  |  |
+------------------+------------------+
     Bottom-left        Bottom-right
```

- **Left buttons**: `m_zoomInButton`, `m_zoomOutButton`, `m_recenterButton` at x=10, stacked vertically
- **Layer button**: Top-right at `width() - 32 - 10`
- **Attribution**: Bottom-left, painted (not a widget), ~8px margin
- **Unmapped panel**: Bottom-right, dynamic size based on unmapped family count

---

## Task 1: Add calculateSafeAreaPadding() Helper Method

**Files:**
- Modify: `src/widgets/MapWidget.h` - Add method declaration
- Modify: `src/widgets/MapWidget.cpp` - Add implementation

**Step 1: Add declaration to header**

In `MapWidget.h`, add in the private section near `ensureVisible`:

```cpp
    // Calculate padding for UI overlays (buttons, panels, attribution)
    QMarginsF calculateSafeAreaPadding() const;
```

**Step 2: Implement the method**

In `MapWidget.cpp`, add after `viewportBounds()`:

```cpp
QMarginsF MapWidget::calculateSafeAreaPadding() const
{
    // Left: zoom/recenter buttons (stacked vertically on left edge)
    double left = m_recenterButton->x() + m_recenterButton->width() + 10;

    // Top: small margin (buttons are on left, layer button on right - top-center is clear)
    double top = 20;

    // Right: unmapped panel width if visible, else layer button area
    // Note: panel grows upward first, so only its width affects right margin
    double right = m_unmappedPanel->isVisible()
        ? (width() - m_unmappedPanel->x() + 10)
        : (m_layerButton->width() + 20);

    // Bottom: attribution area in bottom-left (~30px)
    // Note: unmapped panel is bottom-right, only affects right margin not bottom
    double bottom = 30;

    return QMarginsF(left, top, right, bottom);
}
```

**Step 3: Build and verify compilation**

Run: `build.bat`
Expected: Clean compilation

**Step 4: Commit**

```bash
git add src/widgets/MapWidget.h src/widgets/MapWidget.cpp
git commit -m "feat(map): add calculateSafeAreaPadding() helper for UI overlays"
```

---

## Task 2: Remove MARKER_BUFFER Workaround and Use Safe Area Padding in animateTo()

**Files:**
- Modify: `src/widgets/MapWidget.cpp:1048-1052` - Remove fixed MARKER_BUFFER workaround, use dynamic safe area padding

**Step 1: Update animateTo to use safe area padding**

Remove the temporary `MARKER_BUFFER` constant and replace with proper safe area calculation:

```cpp
    // Available screen space after content padding (for target marker visibility)
    // Add extra buffer (marker icon width) so targets aren't right at the edge
    constexpr double MARKER_BUFFER = 20.0;
    double availableWidth = width() - contentPadding.left() - contentPadding.right() - 2 * MARKER_BUFFER;
    double availableHeight = height() - contentPadding.top() - contentPadding.bottom() - 2 * MARKER_BUFFER;
```

With:

```cpp
    // Calculate safe area that avoids UI overlays (buttons, panels)
    QMarginsF safeArea = calculateSafeAreaPadding();

    // Available screen space = widget size minus UI overlays minus marker padding
    double availableWidth = width() - safeArea.left() - safeArea.right()
                            - contentPadding.left() - contentPadding.right();
    double availableHeight = height() - safeArea.top() - safeArea.bottom()
                             - contentPadding.top() - contentPadding.bottom();
```

**Step 2: Build and verify compilation**

Run: `build.bat`
Expected: Clean compilation

**Step 3: Commit**

```bash
git add src/widgets/MapWidget.cpp
git commit -m "refactor(map): replace MARKER_BUFFER workaround with dynamic safe area padding"
```

---

## Task 3: Update ensureVisible() to Use Safe Area Padding

**Files:**
- Modify: `src/widgets/MapWidget.cpp:844-847` - Use safe area padding for zoom calculation

**Step 1: Update ensureVisible zoom calculation**

Replace:

```cpp
    QMarginsF contentPadding = MarkerRenderer::boundingBox(QVariantMap(), MarkerRenderer::State());

    double availableWidth = width() - contentPadding.left() - contentPadding.right();
    double availableHeight = height() - contentPadding.top() - contentPadding.bottom();
```

With:

```cpp
    QMarginsF markerPadding = MarkerRenderer::boundingBox(QVariantMap(), MarkerRenderer::State());
    QMarginsF safeArea = calculateSafeAreaPadding();

    double availableWidth = width() - safeArea.left() - safeArea.right()
                            - markerPadding.left() - markerPadding.right();
    double availableHeight = height() - safeArea.top() - safeArea.bottom()
                             - markerPadding.top() - markerPadding.bottom();
```

Also update the contentPadding passed to animateTo to combine both:

```cpp
    QMarginsF contentPadding(
        safeArea.left() + markerPadding.left(),
        safeArea.top() + markerPadding.top(),
        safeArea.right() + markerPadding.right(),
        safeArea.bottom() + markerPadding.bottom()
    );
```

**Step 2: Build and verify compilation**

Run: `build.bat`
Expected: Clean compilation

**Step 3: Commit**

```bash
git add src/widgets/MapWidget.cpp
git commit -m "feat(map): use safe area padding in ensureVisible() zoom calculation"
```

---

## Task 4: Update fitAllFamilies() to Use Safe Area Padding

**Files:**
- Modify: `src/widgets/MapWidget.cpp:940-942` - Use safe area padding

**Step 1: Update fitAllFamilies padding**

Find the contentPadding calculation in fitAllFamilies() and update similarly to ensureVisible().

**Step 2: Build and verify compilation**

Run: `build.bat`
Expected: Clean compilation

**Step 3: Commit**

```bash
git add src/widgets/MapWidget.cpp
git commit -m "feat(map): use safe area padding in fitAllFamilies()"
```

---

## Task 5: Manual Testing

**Test scenarios:**

1. Load ward file with unmapped families (panel visible)
2. Zoom in on one area
3. Click families in ward list that are in different directions:
   - One to the upper-left (should not land under zoom buttons)
   - One to the lower-right (should not land under unmapped panel)
   - One to the bottom-left (should not land under attribution)
   - One to the upper-right (should not land under layer button)

4. Test with no unmapped families (panel hidden):
   - Verify targets can use more of the right/bottom area

5. Verify original view stays visible during animation

**Step 1: Run application and test**

Run: Launch app, load ward file, perform test scenarios

**Step 2: Commit any fixes if needed**

---

## Summary

This plan adds dynamic UI-aware padding to map animations by:
1. Creating a helper that calculates actual UI overlay positions
2. Using that padding when calculating zoom levels for animations
3. Ensuring targets land in the visible, unobstructed area of the map
