#pragma once

#include "TileTypes.h"

#include <QObject>
#include <QImage>
#include <QCache>
#include <QSet>

#include <memory>

class TileDiskCacheService;
class TileFetchService;

/// Primary API for tile management - always returns immediately.
///
/// This is a facade that owns TileDiskCacheService (storage) and TileFetchService (network).
/// The renderer calls getTile() which always returns a drawable 256x256 image:
///
/// Priority chain (one level only, no recursion):
/// 1. Memory cache hit - immediate return, no disk I/O
/// 2. Native cached tile - best quality
/// 3. Composite from 4 children at z+1 - full resolution, assembled from cached children
/// 4. Scaled from parent at z-1 - fuzzy, triggers native fetch
/// 5. Gray placeholder - triggers fetch
///
/// After returning, async fetch is triggered if tile is missing, scaled, or stale.
/// When fetch completes, tileReady() is emitted and renderer should repaint.
///
/// Usage:
/// @code
/// TileService* tiles = TileService::instance();
/// tiles->initialize();
/// connect(tiles, &TileService::tileReady, widget, QOverload<>::of(&QWidget::update));
///
/// // In paint event:
/// QImage tile = tiles->getTile(TileId(TileLayer::Street, z, x, y));
/// painter.drawImage(targetRect, tile);
/// @endcode
class TileService : public QObject
{
    Q_OBJECT

public:
    explicit TileService(QObject* parent = nullptr);
    ~TileService() override;

    /// Singleton-like accessor (set by MainWindow)
    static TileService* instance();
    static void setInstance(TileService* instance);

    /// Initialize the service (scans cache, builds index)
    /// Must call before getTile()
    void initialize();

    /// Check if service is initialized
    bool isInitialized() const;

    /// Primary API - always returns immediately.
    /// Returns cached tile, composite, scaled placeholder, or gray placeholder.
    /// Triggers async fetch if needed.
    QImage getTile(TileId id);

    /// Get attribution text for all used providers
    QString attribution() const;

signals:
    /// Emitted when service is initialized
    void initialized();

    /// Emitted when any tile is updated - triggers full repaint.
    /// No arguments because the renderer should just redraw all visible tiles.
    void tileReady();

private slots:
    void onTileFetched(TileId id, const QImage& image, const TileMetadata& metadata);

private:
    /// Try to composite a tile from 4 cached children at z+1.
    /// Returns null image if not all children are cached.
    /// Populates outMeta with oldest child's fetchDate and shared providerId.
    QImage tryComposite(TileId id, TileMetadata* outMeta);

    /// Try to scale a tile from cached parent at z-1.
    /// Returns null image if parent not cached.
    /// Populates outMeta with isScaledUp=true.
    QImage tryScale(TileId id, TileMetadata* outMeta);

    /// Get or create a gray placeholder image.
    QImage grayPlaceholder();

    /// Get the cache directory path (platform-specific)
    static QString getCacheDir();

    std::unique_ptr<TileDiskCacheService> m_diskCache;
    std::unique_ptr<TileFetchService> m_fetchService;

    /// LRU memory cache to avoid disk I/O on repeated getTile() calls.
    /// Covers both layers since preview button shows the other layer.
    static constexpr int MEMORY_CACHE_MAX_TILES = 512;  // ~128MB max
    QCache<TileId, CachedTile> m_memoryCache{MEMORY_CACHE_MAX_TILES};

    QImage m_grayPlaceholder;

    // Track which providers have been used (for attribution)
    QSet<QString> m_usedProviders;
    QString m_cachedAttribution;

    static TileService* s_instance;
};
