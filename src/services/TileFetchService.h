#pragma once

#include "TileTypes.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QImage>
#include <QNetworkAccessManager>
#include <QMap>
#include <QList>
#include <QSet>

class QNetworkReply;

/// Configuration for a tile provider.
struct TileProviderConfig
{
    QString id;              ///< Unique identifier (e.g., "osm", "usgs")
    QString name;            ///< Display name
    QString urlPattern;      ///< URL with {z}, {x}, {y} placeholders
    QString userAgent;       ///< User-Agent header value
    QString attribution;     ///< Attribution text to display
    int minCacheDays = 30;   ///< Minimum days before checking for updates
};

/// Pure network service for fetching map tiles with provider fallback.
///
/// This service only handles network fetching. It does not:
/// - Store tiles (that's TileDiskCacheService's job)
/// - Create placeholders (that's TileService's job)
///
/// Provider fallback:
/// - Each layer has a list of providers in priority order
/// - If primary provider fails, tries next provider
/// - A tile every provider failed is not asked for again for RetryAfterSeconds
///
/// Request deduplication:
/// - Tracks in-flight requests by layer/z/x/y
/// - Ignores duplicate fetch requests for same tile
class TileFetchService : public QObject
{
    Q_OBJECT

public:
    explicit TileFetchService(QObject* parent);
    ~TileFetchService() override;

    /// Initiate async fetch for a tile.
    /// Deduplicates requests - safe to call multiple times for same tile.
    /// The provider that served cached is asked only whether the tile changed, by its ETag
    /// or Last-Modified; when it has not, cached comes back with a new fetch date.
    void fetch(TileId id, const CachedTile& cached);

    /// Get attribution text for a specific provider ID.
    QString providerAttribution(const QString& providerId) const;

    /// Check if fetch is needed based on metadata.
    /// Returns true if:
    /// - meta.isStandIn (built from another zoom, need native)
    /// - !meta.isValid() (no tile at all)
    /// - tile is stale per provider's minCacheDays
    bool needsFetch(TileId id, const TileMetadata& meta) const;

signals:
    /// Emitted when a tile is successfully fetched.
    /// TileService should save this to cache and emit tileReady.
    void tileFetched(TileId id, const QImage& image, const TileMetadata& metadata);

private slots:
    void onRequestFinished(QNetworkReply* reply);

private:
    /// Context for an in-flight network request.
    struct RequestContext
    {
        TileId id;
        int providerIndex;  ///< Index into m_providers[id.layer()]
        CachedTile cached;  ///< the tile held when the fetch began
    };

    void initializeProviders();
    void startFetch(TileId id, int providerIndex, const CachedTile& cached);
    void handleSuccess(const RequestContext& ctx, const QImage& image, const QString& etag,
                       const QString& lastModified);

    QNetworkAccessManager* m_networkManager;

    /// Providers per layer, in fallback order.
    QMap<TileLayer, QList<TileProviderConfig>> m_providers;

    /// Tiles with pending fetch requests (for deduplication).
    QSet<TileId> m_pendingTiles;

    /// When every provider last failed each tile; a success removes its entry.
    QHash<TileId, QDateTime> m_failedAt;
    static constexpr int RetryAfterSeconds = 60;

    /// In-flight network requests.
    QMap<QNetworkReply*, RequestContext> m_inFlightRequests;
};
