# Tile Service Architecture

## Overview

Two services handle tile management for offline-first emergency response mapping:

- **TileCacheService** — Synchronous tile retrieval, always returns a 256×256 pixmap immediately
- **TileFetchService** — Async network fetching, updates cache in background

The renderer never blocks on network. It gets the best available tile instantly, then redraws when better data arrives.

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                          Renderer                                │
│  ┌─────────────────┐                      ┌──────────────────┐  │
│  │ getTile(z,x,y)  │─────────────────────▶│ Draws 256×256    │  │
│  └────────┬────────┘                      │ pixmap returned  │  │
│           │                               └──────────────────┘  │
│           │ ◀──────────────────────────────────────┐            │
│           │                          tileReady(z,x,y) signal    │
└───────────┼────────────────────────────────────────┼────────────┘
            │                                        │
            ▼                                        │
┌───────────────────────────────────────────────────────────────┐
│                     TileCacheService                           │
│                                                                │
│  getTile(z,x,y) → QPixmap                                      │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ 1. Check cache for (z,x,y)          → return if valid    │  │
│  │ 2. Try composite from z+1 children  → cache & return     │  │
│  │ 3. Try scale from z-1 parent        → cache (marked) &   │  │
│  │ 4. Return gray placeholder            return             │  │
│  └──────────────────────────────────────────────────────────┘  │
│                           │                                     │
│                           ▼                                     │
│         notify(TileId, metadata) ──────────────────────────┐    │
│                                                            │    │
│  ◀───── storeTile(TileId, QImage, metadata) ───────────────┼──┐ │
│         emit tileReady(z,x,y)                              │  │ │
└────────────────────────────────────────────────────────────┼──┼─┘
                                                             │  │
                                                             ▼  │
┌───────────────────────────────────────────────────────────────┐
│                     TileFetchService                           │
│                                                                │
│  checkAndFetch(TileId, metadata)                               │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ 1. Is tile missing or stale?                             │  │
│  │ 2. Is tile marked "scaled"?                              │  │
│  │ 3. Try providers in priority order                       │  │
│  │ 4. Handle 512→256 splitting                              │  │
│  │ 5. Store result(s) via TileCacheService                  │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

---

## Tile Normalization: 512×512 → 256×256

All tiles are stored as 256×256 internally. When fetching from a 512×512 provider, split into 4 tiles at the next zoom level. This preserves full resolution and fills the cache faster.

### Coordinate Math

A 512×512 tile at (z, x, y) becomes four 256×256 tiles at z+1:

```
┌─────────────┬─────────────┐
│ (z+1, 2x,   │ (z+1, 2x+1, │
│       2y)   │       2y)   │
├─────────────┼─────────────┤
│ (z+1, 2x,   │ (z+1, 2x+1, │
│       2y+1) │       2y+1) │
└─────────────┴─────────────┘
```

### Implementation

```cpp
void TileFetchService::onTileReceived(TileId requested, QImage image, 
                                       const TileSource& source) {
    if (source.tileSize == 512) {
        // This was a z-1 request, split into 4 tiles at z
        int z1 = requested.z + 1;
        int x2 = requested.x * 2;
        int y2 = requested.y * 2;
        
        cache->storeTile({z1, x2,     y2},     image.copy(0,   0,   256, 256), source);
        cache->storeTile({z1, x2 + 1, y2},     image.copy(256, 0,   256, 256), source);
        cache->storeTile({z1, x2,     y2 + 1}, image.copy(0,   256, 256, 256), source);
        cache->storeTile({z1, x2 + 1, y2 + 1}, image.copy(256, 256, 256, 256), source);
        // Each storeTile emits tileReady
    } else {
        cache->storeTile(requested, image, source);
    }
}
```

### Request Translation

When fetching from a 512px provider, request the parent tile:

```cpp
TileId translateRequest(const TileId& requested, const TileSource& source) {
    if (source.tileSize == 512) {
        return {requested.z - 1, requested.x / 2, requested.y / 2};
    }
    return requested;
}
```

---

## TileCacheService::getTile()

Always returns immediately. Priority chain (one level only, no recursion):

```cpp
QPixmap TileCacheService::getTile(int z, int x, int y) {
    TileId id{z, x, y};
    
    // 1. Native or composited cache hit
    if (auto tile = cache.get(id); tile && !tile->metadata.isScaled) {
        fetchService->checkAndFetch(id, tile->metadata);
        return tile->pixmap;
    }
    
    // 2. Try composite from z+1 children (full resolution)
    if (auto composited = tryComposite(id)) {
        store(id, *composited, Metadata{.isComposited = true});
        fetchService->checkAndFetch(id, Metadata{.isComposited = true});
        return *composited;
    }
    
    // 3. Scaled cache hit (fuzzy, but something)
    if (auto tile = cache.get(id); tile && tile->metadata.isScaled) {
        fetchService->checkAndFetch(id, tile->metadata);
        return tile->pixmap;
    }
    
    // 4. Try scale from z-1 parent (produces fuzzy tile)
    if (auto scaled = tryScale(id)) {
        store(id, *scaled, Metadata{.isScaled = true, .nativeZoom = z - 1});
        fetchService->checkAndFetch(id, Metadata{.isScaled = true});
        return *scaled;
    }
    
    // 5. Gray placeholder
    fetchService->checkAndFetch(id, Metadata{});
    return grayPlaceholder;
}
```

### Priority Summary

```
getTile(z, x, y)
│
├─▶ cache hit (native/composited)? ──▶ return, async check
│
├─▶ 4 children at z+1 all cached? ──▶ composite, store, return, async check
│
├─▶ cache hit (scaled)? ──▶ return, async check
│
├─▶ parent at z-1 cached? ──▶ scale, store, return, async check
│
└─▶ return gray placeholder, async fetch
```

### Why This Order?

- **Composited before scaled**: Composited tiles are full resolution (assembled from real data). Scaled tiles are interpolated and fuzzy.
- **One level only**: No recursive lookups. If z+1 children aren't cached, don't check z+2 grandchildren. Keeps it simple and fast.

---

## Tile Metadata

```cpp
struct TileMetadata {
    QDateTime fetched;          // When we got it
    QString   sourceProvider;   // "osm", "usgs", "arcgis"
    bool      isScaled;         // Derived from parent, not native
    bool      isComposited;     // Derived from children
    int       nativeZoom;       // Original zoom level if scaled
    QDateTime expires;          // Cache-Control / Expires header
    QString   etag;             // For conditional refresh
};
```

---

## TileFetchService

### Fetch Decision Logic

```cpp
bool TileFetchService::shouldFetch(const TileMetadata& meta) {
    // Placeholder - definitely fetch
    if (meta.sourceProvider.isEmpty())
        return true;
    
    // Scaled - fetch because fuzzy
    if (meta.isScaled)
        return true;
    
    // Composited - already full res, lower priority or skip
    if (meta.isComposited)
        return false;
    
    // Stale check
    if (isStale(meta))
        return true;
    
    return false;
}
```

### Provider Priority

Provider priority is **configurable per tile type** (e.g., satellite vs street maps).

Example configuration:

```cpp
struct TileTypeConfig {
    QString tileType;                    // "satellite", "street"
    QVector<QString> providerPriority;   // ["usgs", "arcgis", "osm"]
};
```

### Request Deduplication for 512px Sources

Multiple visible 256px tiles may map to the same 512px parent. Avoid duplicate fetches:

```cpp
QSet<TileId> pending512Requests;

void TileFetchService::requestTile(const TileId& id, const TileSource& source) {
    TileId actualRequest = translateRequest(id, source);
    
    if (source.tileSize == 512) {
        if (pending512Requests.contains(actualRequest))
            return;  // Already in flight
        pending512Requests.insert(actualRequest);
    }
    
    fetchAsync(actualRequest, source);
}

void TileFetchService::onTileReceived(TileId requested, ...) {
    pending512Requests.remove(requested);
    // ... split and store
}
```

---

## Offline Mode

Rather than letting requests fail with timeouts, track network availability:

```cpp
class TileFetchService {
    bool networkAvailable = true;  // Updated by connectivity check
    QSet<TileId> offlineQueue;
    
    void checkAndFetch(TileId id, TileMetadata meta) {
        if (!shouldFetch(meta))
            return;
            
        if (!networkAvailable) {
            offlineQueue.insert(id);
            return;
        }
        
        enqueueRequest(id);
    }
    
    void onNetworkRestored() {
        for (const auto& id : offlineQueue)
            enqueueRequest(id);
        offlineQueue.clear();
    }
};
```

Use `QNetworkInformation` to monitor connectivity state.

---

## Compositing and Scaling Implementation

### Composite from Children (z+1 → z)

```cpp
std::optional<QPixmap> TileCacheService::tryComposite(const TileId& id) {
    int z1 = id.z + 1;
    int x2 = id.x * 2;
    int y2 = id.y * 2;
    
    auto tl = cache.get({z1, x2,     y2});
    auto tr = cache.get({z1, x2 + 1, y2});
    auto bl = cache.get({z1, x2,     y2 + 1});
    auto br = cache.get({z1, x2 + 1, y2 + 1});
    
    if (!tl || !tr || !bl || !br)
        return std::nullopt;
    
    // Assemble 512×512, then scale down to 256×256
    QImage composite(512, 512, QImage::Format_ARGB32);
    QPainter p(&composite);
    p.drawPixmap(0,   0,   tl->pixmap);
    p.drawPixmap(256, 0,   tr->pixmap);
    p.drawPixmap(0,   256, bl->pixmap);
    p.drawPixmap(256, 256, br->pixmap);
    p.end();
    
    return QPixmap::fromImage(composite.scaled(256, 256, 
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}
```

### Scale from Parent (z-1 → z)

```cpp
std::optional<QPixmap> TileCacheService::tryScale(const TileId& id) {
    TileId parentId{id.z - 1, id.x / 2, id.y / 2};
    auto parent = cache.get(parentId);
    
    if (!parent)
        return std::nullopt;
    
    // Determine which quadrant
    int qx = (id.x % 2) * 128;  // 0 or 128
    int qy = (id.y % 2) * 128;  // 0 or 128
    
    // Extract 128×128 quadrant from parent, scale up to 256×256
    QImage quadrant = parent->pixmap.toImage().copy(qx, qy, 128, 128);
    return QPixmap::fromImage(quadrant.scaled(256, 256,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}
```

---

## Signal Flow

```
TileFetchService                    TileCacheService                  Renderer
      │                                    │                              │
      │                                    │◀──── getTile(z,x,y) ─────────│
      │                                    │                              │
      │◀─── checkAndFetch(id, meta) ───────│                              │
      │                                    │───── returns QPixmap ───────▶│
      │                                    │                              │
   [network fetch in background]           │                              │
      │                                    │                              │
      │──── storeTile(id, image, meta) ───▶│                              │
      │                                    │                              │
      │                                    │─── emit tileReady(z,x,y) ───▶│
      │                                    │                              │
      │                                    │◀──── getTile(z,x,y) ─────────│
      │                                    │───── returns new QPixmap ───▶│
```

---

## Open Decisions

- **Storage format**: SQLite? Files? MBTiles?
- **Cache eviction**: LRU? Size-based? Per-provider limits?
- **Staleness policy**: Time-based? ETags? Manual refresh?
