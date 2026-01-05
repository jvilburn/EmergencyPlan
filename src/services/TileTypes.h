#pragma once

#include <QDateTime>
#include <QHashFunctions>
#include <QPixmap>
#include <QString>

#include <cstdint>

/// Map tile layer type
enum class TileLayer { Street, Satellite };

/// Unique identifier for a tile (packed layer/z/x/y).
/// Layout: [layer:1][z:5][x:19][y:19] = 44 bits
struct TileId
{
    uint64_t m_value = 0;

    TileId() = default;
    TileId(TileLayer layer, int z, int x, int y)
        : m_value((static_cast<uint64_t>(layer) << 43)
                | (static_cast<uint64_t>(z) << 38)
                | (static_cast<uint64_t>(x) << 19)
                | static_cast<uint64_t>(y))
    {
    }

    TileLayer layer() const { return static_cast<TileLayer>((m_value >> 43) & 0x1); }
    int zoom() const { return static_cast<int>((m_value >> 38) & 0x1F); }
    int x() const { return static_cast<int>((m_value >> 19) & 0x7FFFF); }
    int y() const { return static_cast<int>(m_value & 0x7FFFF); }

    bool operator==(const TileId& other) const { return m_value == other.m_value; }
};

/// Hash function for TileId (required by QHash, QSet, QCache)
inline size_t qHash(const TileId& id, size_t seed = 0)
{
    return qHash(id.m_value, seed);
}

/// Convert TileLayer to string for paths
inline QString layerName(TileLayer layer)
{
    return layer == TileLayer::Street ? "street" : "satellite";
}

/// Metadata for a cached tile, stored alongside the tile image.
/// Used for conditional requests and staleness checks.
struct TileMetadata
{
    QDateTime fetchDate;      ///< When the tile was downloaded (or oldest source for composites)
    QString etag;             ///< ETag header for conditional requests (empty for scaled/composite)
    QString lastModified;     ///< Last-Modified header for conditional requests (empty for scaled/composite)
    QString providerId;       ///< Provider that served this tile (empty if mixed sources)
    bool isScaledUp = false;  ///< True if scaled from lower zoom - needs native fetch

    bool isValid() const { return fetchDate.isValid(); }
};

/// A cached tile with its pixmap and metadata.
/// Used by both memory cache and disk cache.
struct CachedTile
{
    QPixmap pixmap;
    TileMetadata metadata;
};
