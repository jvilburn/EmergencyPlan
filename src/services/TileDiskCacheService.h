#pragma once

#include "TileTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

#include <optional>

/// How many tiles of each layer the cache keeps outside the area it never trims.
struct TileBudget
{
    int street;
    int satellite;

    /// This device's: a phone keeps a tenth of what a desktop does.
    static TileBudget forDevice();
};

/// Disk cache for map tiles with O(1) lookup using in-memory index.
/// Directory structure: {cacheDir}/{layer}/{z}/{x}/{y}.tile
///
/// A tile file is the tile's pixels in QOI, then its metadata. Tiles in the protected area
/// are never deleted. Outside it each layer keeps at most its budget of tiles: before a new
/// one is saved past the budget, the tiles of that layer drawn least recently go.
class TileDiskCacheService : public QObject
{
    Q_OBJECT

public:
    TileDiskCacheService(const QString& cacheDir, const TileBudget& budget, QObject* parent);
    ~TileDiskCacheService() override;

    /// Initialize the cache service (scans directories, builds index).
    /// Call once before using the service.
    void initialize();

    /// Load tile + metadata together (returns nullopt if not found)
    std::optional<CachedTile> load(TileId id) const;

    /// Save tile + metadata together
    void save(TileId id, const CachedTile& tile);

    /// The tile was drawn: it is now the most recently used of the cache's tiles.
    void markUsed(TileId id);

    /// The tiles never trimmed, as street layer ids: a tile of either layer is protected when
    /// its place is (TileCoverage::placeOf).
    void setProtected(const QSet<TileId>& places);

    bool contains(TileId id) const { return m_tileIndex.contains(id); }

private:
    void scanTileDirectories();
    bool loadIndex();
    void saveIndex();
    void scheduleIndexSave();

    /// Deletes the least recently used unprotected tiles of layer until one more fits in the
    /// budget.
    void makeRoom(TileLayer layer);

    bool isProtected(TileId id) const;
    QString getTilePath(TileId id) const;
    QString getIndexPath() const;

    /// Each cached tile and when it was last used, as a position in the order of use: a
    /// higher number is more recent.
    QHash<TileId, qint64> m_tileIndex;
    qint64 m_lastUse = 0;
    QSet<TileId> m_protected;
    TileBudget m_budget;
    QString m_cacheDir;
    bool m_indexDirty = false;
    QTimer m_indexSaveTimer;
};
