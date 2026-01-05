#include "TileDiskCacheService.h"
#include "QoiCodec.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

TileDiskCacheService::TileDiskCacheService(const QString& cacheDir, QObject* parent)
    : QObject(parent)
    , m_cacheDir(cacheDir)
{
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

    scanTileDirectories();

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

        QDirIterator it(layerDir.absolutePath(), {"*.qoi"}, QDir::Files, QDirIterator::Subdirectories);
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
                int y = parts[2].chopped(4).toInt();
                m_tileIndex.insert(TileId(layer, z, x, y));
            }
        }
    }
}

QString TileDiskCacheService::getTilePath(TileId id) const
{
    return QString("%1/%2/%3/%4/%5.qoi")
        .arg(m_cacheDir, layerName(id.layer()))
        .arg(id.zoom()).arg(id.x()).arg(id.y());
}

std::optional<CachedTile> TileDiskCacheService::load(TileId id) const
{
    if (!m_tileIndex.contains(id))
    {
        return std::nullopt;
    }

    QString tilePath = getTilePath(id);
    QFile file(tilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }

    QImage image = QoiCodec::decode(file.readAll());
    if (image.isNull())
    {
        return std::nullopt;
    }

    TileMetadata meta;
    std::optional<TileMetadata> m = loadMetadata(tilePath + ".meta");
    if (m)
    {
        meta = *m;
    }

    return CachedTile{QPixmap::fromImage(image), meta};
}

void TileDiskCacheService::save(TileId id, const CachedTile& tile)
{
    QString tilePath = getTilePath(id);
    QDir().mkpath(QFileInfo(tilePath).absolutePath());

    QByteArray qoiData = QoiCodec::encode(tile.pixmap.toImage());
    if (qoiData.isEmpty())
    {
        return;
    }

    QFile file(tilePath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(qoiData);
        file.close();

        saveMetadata(tilePath + ".meta", tile.metadata);
        m_tileIndex.insert(id);
    }
}

std::optional<TileMetadata> TileDiskCacheService::loadMetadata(const QString& metaPath) const
{
    QFile file(metaPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
    {
        return std::nullopt;
    }

    QJsonObject obj = doc.object();
    TileMetadata meta;
    meta.fetchDate = QDateTime::fromString(obj["fetchDate"].toString(), Qt::ISODate);
    meta.etag = obj["etag"].toString();
    meta.lastModified = obj["lastModified"].toString();
    meta.providerId = obj["providerId"].toString();
    meta.isScaledUp = obj["isScaledUp"].toBool(false);

    return meta;
}

void TileDiskCacheService::saveMetadata(const QString& metaPath, const TileMetadata& metadata)
{
    QJsonObject obj;
    obj["fetchDate"] = metadata.fetchDate.toString(Qt::ISODate);
    obj["etag"] = metadata.etag;
    obj["lastModified"] = metadata.lastModified;
    obj["providerId"] = metadata.providerId;
    obj["isScaledUp"] = metadata.isScaledUp;

    QFile file(metaPath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
}
