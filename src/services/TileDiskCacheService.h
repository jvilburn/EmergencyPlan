#pragma once

#include "TileTypes.h"

#include <QObject>
#include <QSet>
#include <QTimer>

#include <optional>

/// Disk cache for map tiles with O(1) lookup using in-memory index.
/// Directory structure: {cacheDir}/{layer}/{z}/{x}/{y}.tile
class TileDiskCacheService : public QObject
{
    Q_OBJECT

public:
    explicit TileDiskCacheService(const QString& cacheDir, QObject* parent);
    ~TileDiskCacheService() override;

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
    void scanTileDirectories();
    bool loadIndex();
    void saveIndex();
    void scheduleIndexSave();

    QString getTilePath(TileId id) const;
    QString getIndexPath() const;

    QSet<TileId> m_tileIndex;
    QString m_cacheDir;
    bool m_initialized = false;
    bool m_indexDirty = false;
    QTimer m_indexSaveTimer;
};
