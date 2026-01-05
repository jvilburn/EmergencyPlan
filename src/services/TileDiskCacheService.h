#pragma once

#include "TileTypes.h"

#include <QObject>
#include <QSet>

#include <optional>

/// Disk cache for map tiles with O(1) lookup using in-memory index.
/// Directory structure: {cacheDir}/{layer}/{z}/{x}/{y}.qoi
class TileDiskCacheService : public QObject
{
    Q_OBJECT

public:
    explicit TileDiskCacheService(const QString& cacheDir, QObject* parent = nullptr);
    ~TileDiskCacheService() override = default;

    /// Initialize the cache service (scans directories, builds index).
    /// Call once before using the service.
    void initialize();

    /// Check if service is initialized
    bool isInitialized() const { return m_initialized; }

    /// Load tile + metadata together (returns nullopt if not found)
    std::optional<CachedTile> load(TileId id) const;

    /// Save tile + metadata together
    void save(TileId id, const CachedTile& tile);

private:
    std::optional<TileMetadata> loadMetadata(const QString& metaPath) const;
    void saveMetadata(const QString& metaPath, const TileMetadata& metadata);

    void scanTileDirectories();

    QString getTilePath(TileId id) const;

    QSet<TileId> m_tileIndex;
    QString m_cacheDir;
    bool m_initialized = false;
};
