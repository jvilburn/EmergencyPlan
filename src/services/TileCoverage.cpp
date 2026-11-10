#include "TileCoverage.h"
#include "SlippyMapMath.h"

#include <QtMath>

namespace
{

constexpr double MetersPerDegreeLatitude = 111320.0;

}  // namespace

namespace TileCoverage
{

double radiusMeters(int zoom)
{
    if (zoom >= 18)
    {
        return 250.0;  // the house and its street
    }
    if (zoom == 17)
    {
        return 1000.0;
    }
    if (zoom == 16)
    {
        return 2000.0;
    }
    return 5000.0;
}

QSet<TileId> around(const QList<Coordinates>& positions, int maxZoom)
{
    QSet<TileId> tiles;
    for (const Coordinates& position : positions)
    {
        for (int zoom = 0; zoom <= maxZoom; ++zoom)
        {
            // The square around the circle: it holds a little more than the circle.
            double radius = radiusMeters(zoom);
            double latitudeSpan = radius / MetersPerDegreeLatitude;
            double longitudeSpan = latitudeSpan / qCos(qDegreesToRadians(position.latitude()));
            int count = 1 << zoom;
            auto column = [count](double longitude)
            { return qBound(0, int(qFloor((longitude + 180.0) / 360.0 * count)), count - 1); };
            auto row = [count, zoom](double latitude)
            { return qBound(0, int(qFloor(SlippyMapMath::latToTileY(latitude, zoom))), count - 1); };
            int firstX = column(position.longitude() - longitudeSpan);
            int lastX = column(position.longitude() + longitudeSpan);
            int firstY = row(position.latitude() + latitudeSpan);
            int lastY = row(position.latitude() - latitudeSpan);
            for (int x = firstX; x <= lastX; ++x)
            {
                for (int y = firstY; y <= lastY; ++y)
                {
                    tiles.insert(TileId(TileLayer::Street, zoom, x, y));
                }
            }
        }
    }
    return tiles;
}

}  // namespace TileCoverage
