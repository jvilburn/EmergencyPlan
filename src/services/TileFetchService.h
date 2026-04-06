#pragma once

#include "TileTypes.h"

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
    bool supportsConditional = false;  ///< Supports ETag/If-None-Match
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
/// - Emits fetchFailed only after all providers exhausted
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
    /// @param currentMeta Optional metadata for conditional request (ETag/If-None-Match)
    void fetch(TileId id, const TileMetadata& currentMeta);

    /// Get attribution text for a specific provider ID.
    QString providerAttribution(const QString& providerId) const;

    /// Check if fetch is needed based on metadata.
    /// Returns true if:
    /// - meta.isScaledUp (fuzzy quality, need native)
    /// - !meta.isValid() (no tile at all)
    /// - tile is stale per provider's minCacheDays
    bool needsFetch(TileId id, const TileMetadata& meta) const;

    /// Check if network is currently available.
    bool isNetworkAvailable() const { return m_networkAvailable; }

public slots:
    /// Set network availability. When transitioning to online, flushes queued requests.
    void setNetworkAvailable(bool available);

signals:
    /// Emitted when a tile is successfully fetched.
    /// TileService should save this to cache and emit tileReady.
    void tileFetched(TileId id, const QImage& image, const TileMetadata& metadata);

    /// Emitted when all providers for a tile have failed.
    void fetchFailed(TileId id, const QString& error);

private slots:
    void onRequestFinished(QNetworkReply* reply);

private:
    /// Context for an in-flight network request.
    struct RequestContext
    {
        TileId id;
        int providerIndex;  ///< Index into m_providers[id.layer()]
    };

    void initializeProviders();
    void startFetch(TileId id, int providerIndex, const QString& lastError);
    void handleSuccess(const RequestContext& ctx, const QImage& image,
                       const QString& etag, const QString& lastModified);
    void flushOfflineQueue();

    QNetworkAccessManager* m_networkManager;

    /// Providers per layer, in fallback order.
    QMap<TileLayer, QList<TileProviderConfig>> m_providers;

    /// Tiles with pending fetch requests (for deduplication).
    QSet<TileId> m_pendingTiles;

    /// In-flight network requests.
    QMap<QNetworkReply*, RequestContext> m_inFlightRequests;

    /// Offline mode support
    bool m_networkAvailable = true;
    QSet<TileId> m_offlineQueue;  ///< Tiles to fetch when back online
};
