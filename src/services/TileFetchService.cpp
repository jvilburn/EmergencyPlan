#include "TileFetchService.h"

#include <QCoreApplication>
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
    QString userAgent = "WardPlanning/" + QCoreApplication::applicationVersion();

    // Street maps - OSM primary, ESRI fallback
    m_providers[TileLayer::Street] = {
        TileProviderConfig{
            "osm",
            "OpenStreetMap",
            "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
            userAgent,
            "© OpenStreetMap contributors",
            30     // minCacheDays
        },
        TileProviderConfig{
            "esri-street",
            "ESRI World Street Map",
            "https://services.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer/tile/{z}/{y}/{x}",
            userAgent,
            "© Esri",
            30
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
            30
        },
        TileProviderConfig{
            "esri-imagery",
            "ESRI World Imagery",
            "https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
            userAgent,
            "© Esri",
            30
        }
    };
}

// ============================================================================
// Public API
// ============================================================================

void TileFetchService::fetch(TileId id, const CachedTile& cached)
{
    // Deduplicate - don't fetch if already in flight
    if (m_pendingTiles.contains(id))
    {
        return;
    }
    // A tile every provider just failed waits before it is asked for again, so a repaint
    // while offline or refused asks nothing.
    auto failed = m_failedAt.constFind(id);
    if (failed != m_failedAt.constEnd() && failed->secsTo(QDateTime::currentDateTime()) < RetryAfterSeconds)
    {
        return;
    }
    m_pendingTiles.insert(id);

    // Start with first provider
    startFetch(id, 0, cached);
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
    if (meta.isStandIn)
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

void TileFetchService::startFetch(TileId id, int providerIndex, const CachedTile& cached)
{
    TileLayer layer = id.layer();

    if (!m_providers.contains(layer) || providerIndex >= m_providers[layer].size())
    {
        // Every provider failed
        m_pendingTiles.remove(id);
        m_failedAt.insert(id, QDateTime::currentDateTime());
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

    // Validators belong to the provider that gave them
    if (cached.metadata.providerId == config.id)
    {
        if (!cached.metadata.etag.isEmpty())
        {
            request.setRawHeader("If-None-Match", cached.metadata.etag.toUtf8());
        }
        if (!cached.metadata.lastModified.isEmpty())
        {
            request.setRawHeader("If-Modified-Since", cached.metadata.lastModified.toUtf8());
        }
    }

    QNetworkReply* reply = m_networkManager->get(request);

    m_inFlightRequests.insert(reply, RequestContext{id, providerIndex, cached});
}

void TileFetchService::onRequestFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (!m_inFlightRequests.contains(reply))
    {
        return;
    }

    RequestContext ctx = m_inFlightRequests.take(reply);
    QString etag = QString::fromUtf8(reply->rawHeader("ETag"));
    QString lastModified = QString::fromUtf8(reply->rawHeader("Last-Modified"));
    if (reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 304)
    {
        // Not modified: the tile held is current, and a validator the answer leaves out
        // still holds
        handleSuccess(ctx, ctx.cached.image, etag.isEmpty() ? ctx.cached.metadata.etag : etag,
                      lastModified.isEmpty() ? ctx.cached.metadata.lastModified : lastModified);
        return;
    }
    QImage image;
    if (reply->error() == QNetworkReply::NoError && image.loadFromData(reply->readAll()))
    {
        handleSuccess(ctx, image, etag, lastModified);
        return;
    }

    // Try next provider
    startFetch(ctx.id, ctx.providerIndex + 1, ctx.cached);
}

void TileFetchService::handleSuccess(const RequestContext& ctx, const QImage& image, const QString& etag,
                                     const QString& lastModified)
{
    TileLayer layer = ctx.id.layer();

    TileMetadata meta;
    meta.fetchDate = QDateTime::currentDateTime();
    meta.providerId = m_providers[layer][ctx.providerIndex].id;
    meta.isStandIn = false;
    meta.etag = etag;
    meta.lastModified = lastModified;

    // Remove from pending before emitting (in case slot triggers new fetch)
    m_pendingTiles.remove(ctx.id);
    m_failedAt.remove(ctx.id);

    emit tileFetched(ctx.id, image, meta);
}
