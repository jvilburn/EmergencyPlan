#include "TileFetchService.h"

#include <QNetworkReply>
#include <QNetworkRequest>

TileFetchService::TileFetchService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &TileFetchService::onRequestFinished);

    initializeProviders();
}

TileFetchService::~TileFetchService() = default;

void TileFetchService::initializeProviders()
{
    QString userAgent = "EmergencyPlan/1.0";

    // Street maps - OSM primary, ESRI fallback
    m_providers[TileLayer::Street] = {
        TileProviderConfig{
            "osm",
            "OpenStreetMap",
            "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
            userAgent,
            "© OpenStreetMap contributors",
            30,    // minCacheDays
            true   // supportsConditional
        },
        TileProviderConfig{
            "esri-street",
            "ESRI World Street Map",
            "https://services.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer/tile/{z}/{y}/{x}",
            userAgent,
            "© Esri",
            30,
            false
        }
    };

    // Satellite - USGS primary, ESRI fallback (both use z/y/x order)
    m_providers[TileLayer::Satellite] = {
        TileProviderConfig{
            "usgs",
            "USGS Imagery",
            "https://basemap.nationalmap.gov/arcgis/rest/services/USGSImageryOnly/MapServer/tile/{z}/{y}/{x}",
            userAgent,
            "© USGS The National Map",
            30,
            false
        },
        TileProviderConfig{
            "esri-imagery",
            "ESRI World Imagery",
            "https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
            userAgent,
            "© Esri",
            30,
            false
        }
    };
}

// ============================================================================
// Public API
// ============================================================================

void TileFetchService::fetch(TileId id, const TileMetadata& currentMeta)
{
    Q_UNUSED(currentMeta);  // TODO: Use for conditional requests (If-None-Match)

    // Queue for later if offline
    if (!m_networkAvailable)
    {
        m_offlineQueue.insert(id);
        return;
    }

    // Deduplicate - don't fetch if already in flight
    if (m_pendingTiles.contains(id))
    {
        return;
    }
    m_pendingTiles.insert(id);

    // Start with first provider
    startFetch(id, 0, {});
}

void TileFetchService::setNetworkAvailable(bool available)
{
    if (m_networkAvailable == available)
    {
        return;
    }

    m_networkAvailable = available;

    if (available)
    {
        flushOfflineQueue();
    }
}

void TileFetchService::flushOfflineQueue()
{
    // Swap out queue so fetch() can safely add new entries if network drops again
    QSet<TileId> queue;
    queue.swap(m_offlineQueue);

    for (TileId id : queue)
    {
        fetch(id);
    }
}

QString TileFetchService::providerAttribution(const QString& providerId) const
{
    // Search all layers for this provider ID
    for (auto it = m_providers.begin(); it != m_providers.end(); ++it)
    {
        for (const TileProviderConfig& config : it.value())
        {
            if (config.id == providerId)
            {
                return config.attribution;
            }
        }
    }
    return {};
}

bool TileFetchService::needsFetch(TileId id, const TileMetadata& meta) const
{
    // No tile at all - definitely fetch
    if (!meta.isValid())
    {
        return true;
    }

    // Scaled tile - always fetch to get native resolution
    if (meta.isScaledUp)
    {
        return true;
    }

    // Find the provider config for this tile
    TileLayer layer = id.layer();
    int minCacheDays = 30;  // Default fallback
    bool found = false;

    if (m_providers.contains(layer))
    {
        // Look for the provider that served this tile
        for (const TileProviderConfig& config : m_providers[layer])
        {
            if (config.id == meta.providerId)
            {
                minCacheDays = config.minCacheDays;
                found = true;
                break;
            }
        }
        // If provider not found (switched providers?), use first provider's policy
        if (!found && !m_providers[layer].isEmpty())
        {
            minCacheDays = m_providers[layer].first().minCacheDays;
        }
    }

    QDateTime staleDate = QDateTime::currentDateTime().addDays(-minCacheDays);
    return meta.fetchDate < staleDate;
}

// ============================================================================
// Network handling
// ============================================================================

void TileFetchService::startFetch(TileId id, int providerIndex, const QString& lastError)
{
    TileLayer layer = id.layer();

    if (!m_providers.contains(layer) || providerIndex >= m_providers[layer].size())
    {
        // No more providers to try
        m_pendingTiles.remove(id);
        emit fetchFailed(id, lastError.isEmpty() ? "No providers for layer" : lastError);
        return;
    }

    const TileProviderConfig& config = m_providers[layer][providerIndex];

    // Build URL from pattern
    QString url = config.urlPattern;
    url.replace("{z}", QString::number(id.zoom()));
    url.replace("{x}", QString::number(id.x()));
    url.replace("{y}", QString::number(id.y()));

    QUrl requestUrl(url);
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, config.userAgent);
    request.setRawHeader("Accept", "image/png,image/*;q=0.9,*/*;q=0.8");

    QNetworkReply* reply = m_networkManager->get(request);

    m_inFlightRequests.insert(reply, RequestContext{id, providerIndex});
}

void TileFetchService::onRequestFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (!m_inFlightRequests.contains(reply))
    {
        return;
    }

    RequestContext ctx = m_inFlightRequests.take(reply);
    QString error;

    if (reply->error() != QNetworkReply::NoError)
    {
        error = reply->errorString();
    }
    else if (QByteArray data = reply->readAll(); data.isEmpty())
    {
        error = "Empty response";
    }
    else if (QImage image; !image.loadFromData(data))
    {
        error = "Invalid image data";
    }
    else
    {
        // Success!
        QString etag = QString::fromUtf8(reply->rawHeader("ETag"));
        QString lastModified = QString::fromUtf8(reply->rawHeader("Last-Modified"));
        handleSuccess(ctx, image, etag, lastModified);
        return;
    }

    // Try next provider
    startFetch(ctx.id, ctx.providerIndex + 1, error);
}

void TileFetchService::handleSuccess(const RequestContext& ctx, const QImage& image,
                                      const QString& etag, const QString& lastModified)
{
    TileLayer layer = ctx.id.layer();

    TileMetadata meta;
    meta.fetchDate = QDateTime::currentDateTime();
    meta.providerId = m_providers[layer][ctx.providerIndex].id;
    meta.etag = etag;
    meta.lastModified = lastModified;
    meta.isScaledUp = false;

    // Remove from pending before emitting (in case slot triggers new fetch)
    m_pendingTiles.remove(ctx.id);

    emit tileFetched(ctx.id, image, meta);
}
