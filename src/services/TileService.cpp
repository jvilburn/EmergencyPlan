#include "TileService.h"
#include "TileDiskCacheService.h"
#include "TileFetchService.h"

#include <QStandardPaths>
#include <QPainter>

TileService* TileService::s_instance = nullptr;

TileService::TileService(QObject* parent)
    : QObject(parent)
    , m_diskCache(std::make_unique<TileDiskCacheService>(getCacheDir()))
    , m_fetchService(std::make_unique<TileFetchService>())
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
    emit initialized();
}

bool TileService::isInitialized() const
{
    return m_diskCache->isInitialized();
}

// ============================================================================
// Primary API
// ============================================================================

QPixmap TileService::getTile(TileId id)
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
            QPixmap composited = tryComposite(id, &meta);
            if (!composited.isNull())
            {
                loaded = CachedTile{composited, meta};
                m_diskCache->save(id, *loaded);
            }
            else
            {
                // Try scale from parent at z-1 (fuzzy)
                QPixmap scaled = tryScale(id, &meta);
                if (!scaled.isNull())
                {
                    loaded = CachedTile{scaled, meta};
                    m_diskCache->save(id, *loaded);
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

    // Track provider for attribution
    if (!entry->metadata.providerId.isEmpty())
    {
        m_usedProviders.insert(entry->metadata.providerId);
    }

    // Trigger fetch if needed
    if (m_fetchService->needsFetch(id, entry->metadata))
    {
        m_fetchService->fetch(id, entry->metadata);
    }

    return entry->pixmap;
}

QString TileService::attribution() const
{
    QStringList attributions;
    for (const QString& providerId : m_usedProviders)
    {
        QString attr = m_fetchService->providerAttribution(providerId);
        if (!attr.isEmpty() && !attributions.contains(attr))
        {
            attributions.append(attr);
        }
    }
    return attributions.join(" | ");
}

// ============================================================================
// Tile generation
// ============================================================================

QPixmap TileService::tryComposite(TileId id, TileMetadata* outMeta)
{
    TileLayer layer = id.layer();
    int z = id.zoom();
    int x = id.x();
    int y = id.y();

    // Need all 4 children at z+1
    int childZ = z + 1;
    int childX = x * 2;
    int childY = y * 2;

    // Load all 4 children (returns nullopt if any missing)
    std::optional<CachedTile> tl = m_diskCache->load(TileId(layer, childZ, childX, childY));
    std::optional<CachedTile> tr = m_diskCache->load(TileId(layer, childZ, childX + 1, childY));
    std::optional<CachedTile> bl = m_diskCache->load(TileId(layer, childZ, childX, childY + 1));
    std::optional<CachedTile> br = m_diskCache->load(TileId(layer, childZ, childX + 1, childY + 1));

    if (!tl || !tr || !bl || !br)
    {
        return {};
    }

    if (tl->pixmap.isNull() || tr->pixmap.isNull() || bl->pixmap.isNull() || br->pixmap.isNull())
    {
        return {};
    }

    // Assemble 512x512, then scale down to 256x256
    QImage composite(512, 512, QImage::Format_ARGB32);
    QPainter painter(&composite);
    painter.drawPixmap(0, 0, tl->pixmap);
    painter.drawPixmap(256, 0, tr->pixmap);
    painter.drawPixmap(0, 256, bl->pixmap);
    painter.drawPixmap(256, 256, br->pixmap);
    painter.end();

    QPixmap composited = QPixmap::fromImage(
        composite.scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

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
        outMeta->isScaledUp = false;
    }

    return composited;
}

QPixmap TileService::tryScale(TileId id, TileMetadata* outMeta)
{
    int z = id.zoom();
    if (z <= 0)
    {
        return {};
    }

    TileLayer layer = id.layer();
    int x = id.x();
    int y = id.y();

    int parentZ = z - 1;
    int parentX = x / 2;
    int parentY = y / 2;

    TileId parentId(layer, parentZ, parentX, parentY);
    std::optional<CachedTile> parent = m_diskCache->load(parentId);
    if (!parent || parent->pixmap.isNull())
    {
        return {};
    }

    // Extract quadrant and scale up
    int qx = (x % 2) * 128;
    int qy = (y % 2) * 128;
    QImage quadrant = parent->pixmap.toImage().copy(qx, qy, 128, 128);
    QPixmap scaled = QPixmap::fromImage(
        quadrant.scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    // Populate output metadata - inherit from parent, mark as scaled
    if (outMeta)
    {
        outMeta->fetchDate = parent->metadata.fetchDate.isValid()
            ? parent->metadata.fetchDate
            : QDateTime::currentDateTime();
        outMeta->providerId = parent->metadata.providerId;
        outMeta->isScaledUp = true;
    }

    return scaled;
}

QPixmap TileService::grayPlaceholder()
{
    if (m_grayPlaceholder.isNull())
    {
        QImage placeholder(256, 256, QImage::Format_RGB32);
        placeholder.fill(QColor(220, 220, 220));
        m_grayPlaceholder = QPixmap::fromImage(placeholder);
    }
    return m_grayPlaceholder;
}

// ============================================================================
// Slots
// ============================================================================

void TileService::onTileFetched(TileId id, const QImage& image, const TileMetadata& metadata)
{
    CachedTile tile{QPixmap::fromImage(image), metadata};
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
