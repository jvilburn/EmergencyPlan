# Church Marker Icons Design

## Overview

Add church/chapel markers to the map. Each ward in the document can have chapel coordinates, and these should be displayed as distinct building icons on the map.

## Current State

- `Ward` model has `chapelLat()` and `chapelLng()` fields (populated from church lookup API)
- `Ward` also has `chapelAddress()`, `chapelPhone()`, `meetingTime()` for future tooltips
- `Document` stores `QHash<QString, Ward>` supporting multiple wards
- `MapWidget` currently only draws family markers (orange circles)
- `MarkerRenderer` handles family marker rendering

## Design

### Visual Design

- **Icon:** Opaque chapel-shaped silhouette with interior line art details (door, windows, steeple lines)
- **Color:** Muted warm tone (terracotta/rust, approximately #C2703A)
- **Size:** ~24x24px base, scalable via `state.scale`
- **File:** `src/resources/icons/chapel.svg`

### Rendering

1. Add `MarkerRenderer::drawChurch(painter, pos, ward, state)` function
2. Add `MapWidget::drawChurchMarkers(painter)` method
3. Call `drawChurchMarkers()` before `drawMarkers()` in `paintEvent()` so churches render below families

### Data Flow

```
MapWidget::paintEvent()
  -> drawTiles()
  -> drawChurchMarkers()  // NEW: iterate document.wards(), draw each with valid lat/lng
  -> drawMarkers()        // existing family markers
  -> drawAttribution()
```

### MarkerRenderer State

Reuse existing `State` struct for church markers:
- `scale` - for zoom-based sizing
- `opacity` - for future dimming when highlighting families
- `isHighlighted` - for future tooltip hover effect

## Deferred (Future Work)

- Hit testing for church markers (`hitTestChurch()`)
- `churchClicked(QString wardUnitNumber)` signal
- Tooltip/popup UI showing ward name, address, phone, meeting time

## Files to Modify

| File | Change |
|------|--------|
| `src/resources/icons/chapel.svg` | New SVG icon |
| `src/resources/icons.qrc` | Add chapel.svg to resources |
| `src/widgets/MarkerRenderer.h` | Add `drawChurch()` declaration |
| `src/widgets/MarkerRenderer.cpp` | Add `drawChurch()` implementation |
| `src/widgets/MapWidget.h` | Add `drawChurchMarkers()` declaration |
| `src/widgets/MapWidget.cpp` | Add `drawChurchMarkers()` implementation, call from `paintEvent()` |

## Implementation Steps

1. Create chapel.svg icon asset
2. Add to icons.qrc
3. Add `drawChurch()` to MarkerRenderer
4. Add `drawChurchMarkers()` to MapWidget
5. Test with a document containing ward chapel coordinates
