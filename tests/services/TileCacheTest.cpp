#include "TileCacheTest.h"
#include "SlippyMapMath.h"
#include "TileCoverage.h"
#include "TileDiskCacheService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>
#include <QtMath>

namespace
{

/// A tile with a street map's flat areas and edges: blocks of color with a road across.
QImage streetLikeTile(int seed)
{
    QImage image(256, 256, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(242, 239, 233));
    QPainter painter(&image);
    painter.fillRect(20 + seed, 30, 90, 60, QColor(170, 211, 223));
    painter.fillRect(140, 150 - seed, 80, 70, QColor(205, 235, 176));
    painter.setPen(QPen(QColor(255, 255, 255), 6));
    painter.drawLine(0, 128 + seed, 255, 100);
    painter.end();
    return image;
}

TileMetadata metadata(const QString& etag)
{
    TileMetadata meta;
    meta.fetchDate = QDateTime(QDate(2026, 11, 10), QTime(9, 30), QTimeZone::UTC);
    meta.providerId = "osm";
    meta.etag = etag;
    meta.lastModified = "Tue, 10 Nov 2026 09:30:00 GMT";
    return meta;
}

QString tilePath(const QString& root, TileId id)
{
    return QString("%1/%2/%3/%4/%5.tile").arg(root, layerName(id.layer())).arg(id.zoom()).arg(id.x()).arg(id.y());
}

/// A cache over root that keeps budget tiles of each layer outside the protected area.
std::unique_ptr<TileDiskCacheService> cacheOver(const QString& root, int budget)
{
    auto cache = std::make_unique<TileDiskCacheService>(root, TileBudget{budget, budget}, nullptr);
    cache->initialize();
    return cache;
}

}  // namespace

TileCacheTest::TileCacheTest(QObject* parent)
    : QObject(parent)
{
}

void TileCacheTest::savedTileReadsBackExactlyAndSmaller()
{
    QTemporaryDir folder;
    TileId id(TileLayer::Street, 16, 18760, 25380);
    QImage image = streetLikeTile(3);
    {
        auto cache = cacheOver(folder.path(), 100);
        cache->save(id, CachedTile{image, metadata("\"b18c1f\"")});
    }

    // A new cache over the same folder reads the tile from its index and file
    auto cache = cacheOver(folder.path(), 100);
    std::optional<CachedTile> loaded = cache->load(id);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->image.format(), QImage::Format_ARGB32_Premultiplied);
    QVERIFY(loaded->image == image);
    QCOMPARE(loaded->metadata.etag, QString("\"b18c1f\""));
    QCOMPARE(loaded->metadata.lastModified, QString("Tue, 10 Nov 2026 09:30:00 GMT"));
    QCOMPARE(loaded->metadata.providerId, QString("osm"));
    QCOMPARE(loaded->metadata.fetchDate, metadata(QString()).fetchDate);

    QFile file(tilePath(folder.path(), id));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.read(4), QByteArray("qoif"));
    QVERIFY2(file.size() < 256 * 256 * 4 / 10, qPrintable(QString::number(file.size())));
}

void TileCacheTest::newTilePastTheBudgetDeletesTheLeastRecentlyUsed()
{
    QTemporaryDir folder;
    auto cache = cacheOver(folder.path(), 2);
    TileId first(TileLayer::Street, 15, 100, 200);
    TileId second(TileLayer::Street, 15, 101, 200);
    TileId third(TileLayer::Street, 15, 102, 200);
    TileId satellite(TileLayer::Satellite, 15, 100, 200);
    cache->save(first, CachedTile{streetLikeTile(1), metadata("a")});
    cache->save(second, CachedTile{streetLikeTile(2), metadata("b")});
    cache->save(satellite, CachedTile{streetLikeTile(4), metadata("d")});
    cache->markUsed(first);

    cache->save(third, CachedTile{streetLikeTile(3), metadata("c")});

    QVERIFY(cache->contains(first));
    QVERIFY(!cache->contains(second));
    QVERIFY(!QFileInfo::exists(tilePath(folder.path(), second)));
    QVERIFY(cache->contains(third));
    // The other layer has its own budget
    QVERIFY(cache->contains(satellite));

    // Saving a tile again takes no room
    cache->save(first, CachedTile{streetLikeTile(1), metadata("a2")});
    QVERIFY(cache->contains(first));
    QVERIFY(cache->contains(third));
}

void TileCacheTest::protectedTilesAreNeitherDeletedNorCounted()
{
    QTemporaryDir folder;
    auto cache = cacheOver(folder.path(), 1);
    TileId home(TileLayer::Satellite, 17, 500, 600);
    TileId away(TileLayer::Satellite, 17, 900, 900);
    TileId further(TileLayer::Satellite, 17, 901, 900);
    cache->setProtected({TileCoverage::placeOf(home)});

    cache->save(home, CachedTile{streetLikeTile(1), metadata("h")});
    cache->save(away, CachedTile{streetLikeTile(2), metadata("a")});
    QVERIFY(cache->contains(home));
    QVERIFY(cache->contains(away));

    cache->save(further, CachedTile{streetLikeTile(3), metadata("f")});

    QVERIFY(cache->contains(home));
    QVERIFY(!cache->contains(away));
    QVERIFY(cache->contains(further));
}

void TileCacheTest::coverageHoldsAHouseholdsStreetInDetailAndItsTownCoarsely()
{
    // A household in Clemmons, NC, and a point about 1.5 km east of it
    Coordinates home(36.0215, -80.3820);
    double east = -80.3820 + 1.5 / (111.32 * qCos(qDegreesToRadians(36.0215)));
    QSet<TileId> covered = TileCoverage::around({home}, 19);
    auto tileAt = [](double latitude, double longitude, int zoom)
    {
        int count = 1 << zoom;
        return TileId(TileLayer::Street, zoom, int(qFloor((longitude + 180.0) / 360.0 * count)),
                      int(qFloor(SlippyMapMath::latToTileY(latitude, zoom))));
    };

    for (int zoom = 0; zoom <= 19; ++zoom)
    {
        QVERIFY2(covered.contains(tileAt(36.0215, -80.3820, zoom)), qPrintable(QString::number(zoom)));
    }
    // 1.5 km away: outside the street-level circles, inside the coarser ones
    QVERIFY(!covered.contains(tileAt(36.0215, east, 19)));
    QVERIFY(!covered.contains(tileAt(36.0215, east, 18)));
    QVERIFY(!covered.contains(tileAt(36.0215, east, 17)));
    QVERIFY(covered.contains(tileAt(36.0215, east, 16)));
    QVERIFY(covered.contains(tileAt(36.0215, east, 14)));
    QVERIFY(TileCoverage::placeOf(TileId(TileLayer::Satellite, 16, 7, 9)) == TileId(TileLayer::Street, 16, 7, 9));
}
