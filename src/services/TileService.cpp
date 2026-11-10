#include "TileService.h"
#include "TileCoverage.h"
#include "TileDiskCacheService.h"
#include "TileFetchService.h"

#include <QStandardPaths>
#include <QPainter>
#include <QDebug>

TileService* TileService::s_instance = nullptr;

TileService::TileService(QObject* parent)
    : QObject(parent)
    , m_diskCache(std::make_unique<TileDiskCacheService>(getCacheDir(), TileBudget::forDevice(), this))
    , m_fetchService(std::make_unique<TileFetchService>(this))
{
    connect(m_fetchService.get(), &TileFetchService::tileFetched,
            this, &TileService::onTileFetched);
}

TileService::~TileService() = default;

TileService* TileService::instance()
{
    return s_instance;
}

void TileService::setInstance(TileService* instance)
{
    s_instance = instance;
}

void TileService::initialize()
{
    m_diskCache->initialize();
}

// ============================================================================
// Primary API
// ============================================================================

QImage TileService::getTile(TileId id)
{
    // Check memory cache first (no disk I/O)
    CachedTile* entry = m_memoryCache.object(id);

    if (!entry)
    {
        // Cache miss - load from disk or generate
        std::optional<CachedTile> loaded = m_diskCache->load(id);

        if (!loaded)
        {
            // Try composite from 4 children at z+1 (full resolution)
            TileMetadata meta;
            QImage composited = tryComposite(id, &meta);

            if (!composited.isNull())
            {
                loaded = CachedTile{composited, meta};
                m_diskCache->save(id, *loaded);
            }
            else
            {
                // Try scale from parent at z-1 (fuzzy)
                QImage scaled = tryScale(id, &meta);

                if (!scaled.isNull())
                {
                    // Don't save to disk - scaled tiles are temporary placeholders.
                    // The fetch service will retrieve the native tile shortly.
                    loaded = CachedTile{scaled, meta};
                }
                else
                {
                    // Nothing available - return placeholder
                    loaded = CachedTile{grayPlaceholder(), TileMetadata{}};
                }
            }
        }

        // Insert into memory cache
        entry = new CachedTile{*loaded};
        m_memoryCache.insert(id, entry);
    }

    m_diskCache->markUsed(id);

    // Track provider for attribution (rebuild cached string only when set grows)
    if (!entry->metadata.providerId.isEmpty()
        && !m_usedProviders.contains(entry->metadata.providerId))
    {
        m_usedProviders.insert(entry->metadata.providerId);
        QStringList parts;
        for (const QString& pid : m_usedProviders)
        {
            QString attr = m_fetchService->providerAttribution(pid);
            if (!attr.isEmpty() && !parts.contains(attr))
            {
                parts.append(attr);
            }
        }
        m_cachedAttribution = parts.join(" | ");
    }

    // Trigger fetch if needed
    if (m_fetchService->needsFetch(id, entry->metadata))
    {
        m_fetchService->fetch(id, *entry);
    }

    return entry->image;
}

QString TileService::attribution() const
{
    return m_cachedAttribution;
}

void TileService::protectAround(const QList<Coordinates>& households, int maxZoom)
{
    // Most document changes move no household, and the tiles take a moment to work out.
    if (households == m_protectedHouseholds)
    {
        return;
    }
    m_protectedHouseholds = households;
    m_diskCache->setProtected(TileCoverage::around(households, maxZoom));
}

// ============================================================================
// Tile generation
// ============================================================================

QImage TileService::tryComposite(TileId id, TileMetadata* outMeta)
{
    TileLayer layer = id.layer();
    int z = id.zoom();
    int x = id.x();
    int y = id.y();

    // Need all 4 children at z+1
    int childZ = z + 1;
    int childX = x * 2;
    int childY = y * 2;

    // All 4 children, stopping at the first one missing so no more are decoded
    std::optional<CachedTile> tl, tr, bl, br;
    if (!(tl = cachedTile(TileId(layer, childZ, childX, childY)))
        || !(tr = cachedTile(TileId(layer, childZ, childX + 1, childY)))
        || !(bl = cachedTile(TileId(layer, childZ, childX, childY + 1)))
        || !(br = cachedTile(TileId(layer, childZ, childX + 1, childY + 1))))
    {
        return {};
    }

    // Assemble 512x512, then scale down to 256x256
    QImage composite(512, 512, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&composite);
    painter.drawImage(0, 0, tl->image);
    painter.drawImage(256, 0, tr->image);
    painter.drawImage(0, 256, bl->image);
    painter.drawImage(256, 256, br->image);
    painter.end();

    QImage composited = composite.scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Populate output metadata
    if (outMeta)
    {
        // Find oldest fetchDate
        QDateTime oldest = QDateTime::currentDateTime();
        for (const std::optional<CachedTile>& child : {tl, tr, bl, br})
        {
            if (child->metadata.fetchDate.isValid() && child->metadata.fetchDate < oldest)
            {
                oldest = child->metadata.fetchDate;
            }
        }
        outMeta->fetchDate = oldest;

        // Check if all children share the same provider
        QString sharedProvider;
        bool allSameProvider = true;
        for (const std::optional<CachedTile>& child : {tl, tr, bl, br})
        {
            if (!child->metadata.providerId.isEmpty())
            {
                if (sharedProvider.isEmpty())
                {
                    sharedProvider = child->metadata.providerId;
                }
                else if (sharedProvider != child->metadata.providerId)
                {
                    allSameProvider = false;
                    break;
                }
            }
        }
        if (allSameProvider)
        {
            outMeta->providerId = sharedProvider;
        }
        // Kept for offline use, and replaced by the real tile once it can be fetched
        outMeta->isStandIn = true;
    }

    return composited;
}

QImage TileService::tryScale(TileId id, TileMetadata* outMeta)
{
    // The nearest cached tile above, by as many levels as it takes while its part over this
    // tile is at least a pixel, enlarged
    for (int levels = 1; levels <= id.zoom() && (256 >> levels) >= 1; ++levels)
    {
        std::optional<CachedTile> ancestor =
            cachedTile(TileId(id.layer(), id.zoom() - levels, id.x() >> levels, id.y() >> levels));
        if (!ancestor)
        {
            continue;
        }

        int size = 256 >> levels;
        int mask = (1 << levels) - 1;
        QImage part = ancestor->image.copy((id.x() & mask) * size, (id.y() & mask) * size, size, size);
        QImage scaled = part.scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        if (outMeta)
        {
            outMeta->fetchDate = ancestor->metadata.fetchDate.isValid()
                ? ancestor->metadata.fetchDate
                : QDateTime::currentDateTime();
            outMeta->providerId = ancestor->metadata.providerId;
            outMeta->isStandIn = true;
        }
        return scaled;
    }
    return {};
}

std::optional<CachedTile> TileService::cachedTile(TileId id)
{
    // A gray placeholder in memory holds no tile
    CachedTile* entry = m_memoryCache.object(id);
    if (entry && entry->metadata.isValid())
    {
        return *entry;
    }
    return m_diskCache->load(id);
}

QImage TileService::grayPlaceholder()
{
    if (m_grayPlaceholder.isNull())
    {
        m_grayPlaceholder = QImage(256, 256, QImage::Format_ARGB32_Premultiplied);
        m_grayPlaceholder.fill(QColor(220, 220, 220));
    }
    return m_grayPlaceholder;
}

// ============================================================================
// Slots
// ============================================================================

void TileService::onTileFetched(TileId id, const QImage& image, const TileMetadata& metadata)
{
    CachedTile tile{image, metadata};
    m_diskCache->save(id, tile);

    // Invalidate memory cache so next getTile() loads fresh tile
    m_memoryCache.remove(id);

    emit tileReady();
}

// ============================================================================
// Utilities
// ============================================================================

QString TileService::getCacheDir()
{
    QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return cacheLocation + "/tiles";
}
