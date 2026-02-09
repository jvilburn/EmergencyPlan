#include "TileDiskCacheService.h"

#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstring>

static constexpr int INDEX_SAVE_DELAY_MS = 5000;

/// POD struct for on-disk tile metadata — appended after pixel data.
/// Must be trivially copyable (no Qt types, no pointers).
struct TileMetaDisk
{
    int64_t fetchEpochMs;       ///< QDateTime::toMSecsSinceEpoch()
    char providerId[16];        ///< e.g. "osm", "usgs", "esri-street"
    char etag[64];              ///< ETag header (often empty)
    char lastModified[64];      ///< Last-Modified header (often empty)
    uint8_t isScaledUp;         ///< bool
    uint8_t reserved[7];        ///< padding for future use
};
static_assert(std::is_trivially_copyable_v<TileMetaDisk>);

static constexpr qint64 PIXEL_BYTES = 256 * 256 * 4;
static constexpr qint64 EXPECTED_SIZE = PIXEL_BYTES + sizeof(TileMetaDisk);

static TileMetaDisk toMetaDisk(const TileMetadata& meta)
{
    TileMetaDisk d{};
    d.fetchEpochMs = meta.fetchDate.isValid() ? meta.fetchDate.toMSecsSinceEpoch() : 0;
    qstrncpy(d.providerId, meta.providerId.toUtf8().constData(), sizeof(d.providerId));
    qstrncpy(d.etag, meta.etag.toUtf8().constData(), sizeof(d.etag));
    qstrncpy(d.lastModified, meta.lastModified.toUtf8().constData(), sizeof(d.lastModified));
    d.isScaledUp = meta.isScaledUp ? 1 : 0;
    return d;
}

static TileMetadata fromMetaDisk(const TileMetaDisk& d)
{
    TileMetadata meta;
    if (d.fetchEpochMs != 0)
    {
        meta.fetchDate = QDateTime::fromMSecsSinceEpoch(d.fetchEpochMs, Qt::UTC);
    }
    meta.providerId = QString::fromUtf8(d.providerId, strnlen(d.providerId, sizeof(d.providerId)));
    meta.etag = QString::fromUtf8(d.etag, strnlen(d.etag, sizeof(d.etag)));
    meta.lastModified = QString::fromUtf8(d.lastModified, strnlen(d.lastModified, sizeof(d.lastModified)));
    meta.isScaledUp = d.isScaledUp != 0;
    return meta;
}

TileDiskCacheService::TileDiskCacheService(const QString& cacheDir, QObject* parent)
    : QObject(parent)
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
    if (m_initialized)
    {
        return;
    }

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

    m_initialized = true;
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
                m_tileIndex.insert(TileId(layer, z, x, y));
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

    m_tileIndex.clear();
    QJsonArray arr = doc.array();
    for (const QJsonValue& val : arr)
    {
        QJsonArray tile = val.toArray();
        if (tile.size() == 4)
        {
            TileLayer layer = static_cast<TileLayer>(tile[0].toInt());
            int z = tile[1].toInt();
            int x = tile[2].toInt();
            int y = tile[3].toInt();
            m_tileIndex.insert(TileId(layer, z, x, y));
        }
    }

    return true;
}

void TileDiskCacheService::saveIndex()
{
    QElapsedTimer stepTimer;
    stepTimer.start();

    QJsonArray arr;
    for (const TileId& id : m_tileIndex)
    {
        QJsonArray tile;
        tile.append(static_cast<int>(id.layer()));
        tile.append(id.zoom());
        tile.append(id.x());
        tile.append(id.y());
        arr.append(tile);
    }

    QFile file(getIndexPath());
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    }

    m_indexDirty = false;

    qDebug() << "    saveIndex:" << stepTimer.nsecsElapsed() / 1000 << "us"
             << "tiles:" << m_tileIndex.size();
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

    QElapsedTimer stepTimer;

    stepTimer.start();
    QString tilePath = getTilePath(id);
    qint64 pathUs = stepTimer.nsecsElapsed() / 1000;

    stepTimer.start();
    QFile file(tilePath);
    qint64 ctorUs = stepTimer.nsecsElapsed() / 1000;

    stepTimer.start();
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    qint64 openUs = stepTimer.nsecsElapsed() / 1000;

    stepTimer.start();
    QByteArray data = file.readAll();
    qint64 readUs = stepTimer.nsecsElapsed() / 1000;

    if (data.size() < PIXEL_BYTES)
    {
        return std::nullopt;
    }

    stepTimer.start();
    QImage image(
        reinterpret_cast<const uchar*>(data.constData()),
        256, 256,
        256 * 4,
        QImage::Format_ARGB32_Premultiplied);
    QImage owned = image.copy();  // detach from QByteArray — single memcpy
    qint64 copyUs = stepTimer.nsecsElapsed() / 1000;

    stepTimer.start();
    TileMetadata meta;
    if (data.size() >= EXPECTED_SIZE)
    {
        TileMetaDisk d;
        memcpy(&d, data.constData() + PIXEL_BYTES, sizeof(TileMetaDisk));
        meta = fromMetaDisk(d);
    }
    qint64 metaUs = stepTimer.nsecsElapsed() / 1000;

    qDebug() << "    diskLoad: path:" << pathUs << "ctor:" << ctorUs
             << "open:" << openUs << "read:" << readUs
             << "copy:" << copyUs << "meta:" << metaUs
             << "bytes:" << data.size();

    return CachedTile{owned, meta};
}

void TileDiskCacheService::save(TileId id, const CachedTile& tile)
{
    QElapsedTimer stepTimer;

    stepTimer.start();
    QString tilePath = getTilePath(id);
    QDir().mkpath(QFileInfo(tilePath).absolutePath());
    qint64 mkpathUs = stepTimer.nsecsElapsed() / 1000;

    stepTimer.start();
    QImage image = tile.image;
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
    {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    qint64 convertUs = stepTimer.nsecsElapsed() / 1000;

    if (image.isNull() || image.width() != 256 || image.height() != 256)
    {
        return;
    }

    QFile file(tilePath);
    if (file.open(QIODevice::WriteOnly))
    {
        stepTimer.start();
        file.write(reinterpret_cast<const char*>(image.constBits()), PIXEL_BYTES);

        TileMetaDisk d = toMetaDisk(tile.metadata);
        file.write(reinterpret_cast<const char*>(&d), sizeof(d));
        file.close();
        qint64 writeUs = stepTimer.nsecsElapsed() / 1000;

        bool isNew = !m_tileIndex.contains(id);
        m_tileIndex.insert(id);
        if (isNew)
        {
            scheduleIndexSave();
        }

        qDebug() << "    diskSave: mkpath:" << mkpathUs << "convert:" << convertUs
                 << "write:" << writeUs
                 << "bytes:" << EXPECTED_SIZE;
    }
}

