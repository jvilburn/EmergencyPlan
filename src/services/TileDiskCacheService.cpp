#include "TileDiskCacheService.h"
#include "TileCoverage.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "qoi.h"

static constexpr int INDEX_SAVE_DELAY_MS = 5000;

/// POD struct for on-disk tile metadata, appended after the pixel data.
/// Must be trivially copyable (no Qt types, no pointers).
struct TileMetaDisk
{
    int64_t fetchEpochMs;       ///< QDateTime::toMSecsSinceEpoch()
    char providerId[16];        ///< e.g. "osm", "usgs", "esri-street"
    char etag[64];              ///< ETag header (often empty)
    char lastModified[64];      ///< Last-Modified header (often empty)
    uint8_t isStandIn;          ///< bool
    uint8_t reserved[7];        ///< padding for future use
};
static_assert(std::is_trivially_copyable_v<TileMetaDisk>);

static TileMetaDisk toMetaDisk(const TileMetadata& meta)
{
    TileMetaDisk d{};
    d.fetchEpochMs = meta.fetchDate.isValid() ? meta.fetchDate.toMSecsSinceEpoch() : 0;
    qstrncpy(d.providerId, meta.providerId.toUtf8().constData(), sizeof(d.providerId));
    qstrncpy(d.etag, meta.etag.toUtf8().constData(), sizeof(d.etag));
    qstrncpy(d.lastModified, meta.lastModified.toUtf8().constData(), sizeof(d.lastModified));
    d.isStandIn = meta.isStandIn ? 1 : 0;
    return d;
}

static TileMetadata fromMetaDisk(const TileMetaDisk& d)
{
    TileMetadata meta;
    if (d.fetchEpochMs != 0)
    {
        meta.fetchDate = QDateTime::fromMSecsSinceEpoch(d.fetchEpochMs, QTimeZone::UTC);
    }
    meta.providerId = QString::fromUtf8(d.providerId, strnlen(d.providerId, sizeof(d.providerId)));
    meta.etag = QString::fromUtf8(d.etag, strnlen(d.etag, sizeof(d.etag)));
    meta.lastModified = QString::fromUtf8(d.lastModified, strnlen(d.lastModified, sizeof(d.lastModified)));
    meta.isStandIn = d.isStandIn != 0;
    return meta;
}

TileBudget TileBudget::forDevice()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return TileBudget{8000, 1000};
#else
    return TileBudget{80000, 10000};
#endif
}

TileDiskCacheService::TileDiskCacheService(const QString& cacheDir, const TileBudget& budget, QObject* parent)
    : QObject(parent)
    , m_budget(budget)
    , m_cacheDir(cacheDir)
{
    m_indexSaveTimer.setSingleShot(true);
    connect(&m_indexSaveTimer, &QTimer::timeout, this, &TileDiskCacheService::saveIndex);
}

TileDiskCacheService::~TileDiskCacheService()
{
    // Flush dirty index on shutdown
    if (m_indexDirty)
    {
        saveIndex();
    }
}

void TileDiskCacheService::initialize()
{
    QDir dir(m_cacheDir);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    dir.mkpath("street");
    dir.mkpath("satellite");

    // Try loading persisted index first (fast path)
    if (!loadIndex())
    {
        // Fall back to directory scan (slow path, rebuilds index)
        scanTileDirectories();
        saveIndex();
    }
}

void TileDiskCacheService::scanTileDirectories()
{
    m_tileIndex.clear();

    for (TileLayer layer : {TileLayer::Street, TileLayer::Satellite})
    {
        QDir layerDir(m_cacheDir + "/" + layerName(layer));
        if (!layerDir.exists())
        {
            continue;
        }

        QDirIterator it(layerDir.absolutePath(), {"*.tile"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            it.next();
            QString relativePath = layerDir.relativeFilePath(it.filePath());
            relativePath = relativePath.replace('\\', '/');

            QStringList parts = relativePath.split('/');
            if (parts.size() == 3)
            {
                int z = parts[0].toInt();
                int x = parts[1].toInt();
                int y = parts[2].chopped(5).toInt();
                // With no record of use, a scanned tile is the least recently used.
                m_tileIndex.insert(TileId(layer, z, x, y), 0);
            }
        }
    }
}

QString TileDiskCacheService::getIndexPath() const
{
    return m_cacheDir + "/index.json";
}

bool TileDiskCacheService::loadIndex()
{
    QFile file(getIndexPath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
    {
        return false;
    }

    // Each entry is [layer, z, x, y, last use]; an index with any other entry is rebuilt by
    // scanning
    m_tileIndex.clear();
    m_lastUse = 0;
    QJsonArray arr = doc.array();
    for (const QJsonValue& val : arr)
    {
        QJsonArray tile = val.toArray();
        if (tile.size() != 5)
        {
            m_tileIndex.clear();
            m_lastUse = 0;
            return false;
        }
        TileLayer layer = static_cast<TileLayer>(tile[0].toInt());
        qint64 use = tile[4].toInteger();
        m_tileIndex.insert(TileId(layer, tile[1].toInt(), tile[2].toInt(), tile[3].toInt()), use);
        m_lastUse = qMax(m_lastUse, use);
    }

    return true;
}

void TileDiskCacheService::saveIndex()
{
    QJsonArray arr;
    for (auto it = m_tileIndex.constBegin(); it != m_tileIndex.constEnd(); ++it)
    {
        QJsonArray tile;
        tile.append(static_cast<int>(it.key().layer()));
        tile.append(it.key().zoom());
        tile.append(it.key().x());
        tile.append(it.key().y());
        tile.append(it.value());
        arr.append(tile);
    }

    QFile file(getIndexPath());
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    }

    m_indexDirty = false;
}

void TileDiskCacheService::scheduleIndexSave()
{
    m_indexDirty = true;
    m_indexSaveTimer.start(INDEX_SAVE_DELAY_MS);
}

QString TileDiskCacheService::getTilePath(TileId id) const
{
    return QString("%1/%2/%3/%4/%5.tile")
        .arg(m_cacheDir, layerName(id.layer()))
        .arg(id.zoom()).arg(id.x()).arg(id.y());
}

std::optional<CachedTile> TileDiskCacheService::load(TileId id) const
{
    if (!m_tileIndex.contains(id))
    {
        return std::nullopt;
    }

    QFile file(getTilePath(id));
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    QByteArray bytes = file.readAll();

    // QOI pixels, then the metadata. A file that does not decode is a tile not cached: it is
    // fetched again and overwritten.
    qsizetype pixelBytes = bytes.size() - qsizetype(sizeof(TileMetaDisk));
    if (pixelBytes <= 0)
    {
        return std::nullopt;
    }
    qoi_desc desc;
    void* pixels = qoi_decode(bytes.constData(), int(pixelBytes), &desc, 4);
    if (!pixels)
    {
        return std::nullopt;
    }
    if (desc.width != 256 || desc.height != 256)
    {
        std::free(pixels);
        return std::nullopt;
    }
    // The image takes the decoded buffer as its own, and frees it.
    QImage image(static_cast<uchar*>(pixels), 256, 256, 256 * 4, QImage::Format_ARGB32_Premultiplied,
                 [](void* buffer) { std::free(buffer); }, pixels);
    TileMetaDisk d{};
    std::memcpy(&d, bytes.constData() + pixelBytes, sizeof(TileMetaDisk));
    return CachedTile{image, fromMetaDisk(d)};
}

void TileDiskCacheService::save(TileId id, const CachedTile& tile)
{
    QImage image = tile.image;
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
    {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    if (image.isNull() || image.width() != 256 || image.height() != 256)
    {
        return;
    }

    if (!m_tileIndex.contains(id) && !isProtected(id))
    {
        makeRoom(id.layer());
    }

    // QOI over the image's own bytes, which it stores losslessly whatever their order
    qoi_desc desc{256, 256, 4, QOI_SRGB};
    int encodedBytes = 0;
    void* encoded = qoi_encode(image.constBits(), &desc, &encodedBytes);
    if (!encoded)
    {
        return;
    }

    QString tilePath = getTilePath(id);
    QDir().mkpath(QFileInfo(tilePath).absolutePath());
    QFile file(tilePath);
    bool written = false;
    if (file.open(QIODevice::WriteOnly))
    {
        TileMetaDisk d = toMetaDisk(tile.metadata);
        written = file.write(static_cast<const char*>(encoded), encodedBytes) == encodedBytes
            && file.write(reinterpret_cast<const char*>(&d), sizeof(d)) == qint64(sizeof(d));
        file.close();
    }
    std::free(encoded);

    if (written)
    {
        m_tileIndex.insert(id, ++m_lastUse);
        scheduleIndexSave();
    }
}

void TileDiskCacheService::markUsed(TileId id)
{
    auto it = m_tileIndex.find(id);
    if (it == m_tileIndex.end() || it.value() == m_lastUse)
    {
        return;
    }
    it.value() = ++m_lastUse;
    scheduleIndexSave();
}

void TileDiskCacheService::setProtected(const QSet<TileId>& places)
{
    m_protected = places;
}

bool TileDiskCacheService::isProtected(TileId id) const
{
    return m_protected.contains(TileCoverage::placeOf(id));
}

void TileDiskCacheService::makeRoom(TileLayer layer)
{
    int budget = layer == TileLayer::Street ? m_budget.street : m_budget.satellite;
    QList<std::pair<qint64, TileId>> trimmable;
    for (auto it = m_tileIndex.constBegin(); it != m_tileIndex.constEnd(); ++it)
    {
        if (it.key().layer() == layer && !isProtected(it.key()))
        {
            trimmable.append({it.value(), it.key()});
        }
    }
    qsizetype excess = trimmable.size() - budget + 1;
    if (excess <= 0)
    {
        return;
    }

    auto byUse = [](const std::pair<qint64, TileId>& a, const std::pair<qint64, TileId>& b) { return a.first < b.first; };
    std::nth_element(trimmable.begin(), trimmable.begin() + (excess - 1), trimmable.end(), byUse);
    for (qsizetype i = 0; i < excess; ++i)
    {
        QFile::remove(getTilePath(trimmable[i].second));
        m_tileIndex.remove(trimmable[i].second);
    }
    scheduleIndexSave();
}
