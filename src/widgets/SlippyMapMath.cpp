#include "SlippyMapMath.h"

#include <QtMath>
#include <cmath>

namespace SlippyMapMath
{

double latToTileY(double lat, int zoom)
{
    double latRad = qDegreesToRadians(lat);
    double n = 1 << zoom;
    return (1.0 - qLn(qTan(latRad) + 1.0 / qCos(latRad)) / M_PI) / 2.0 * n;
}

double latToTileY(double lat, double zoom)
{
    double latRad = qDegreesToRadians(lat);
    double n = qPow(2.0, zoom);
    return (1.0 - qLn(qTan(latRad) + 1.0 / qCos(latRad)) / M_PI) / 2.0 * n;
}

double tileYToLat(double tileY, int zoom)
{
    double n = 1 << zoom;
    double latRad = std::atan(std::sinh(M_PI * (1.0 - 2.0 * tileY / n)));
    return qRadiansToDegrees(latRad);
}

double tileYToLat(double tileY, double zoom)
{
    double n = qPow(2.0, zoom);
    double latRad = std::atan(std::sinh(M_PI * (1.0 - 2.0 * tileY / n)));
    return qRadiansToDegrees(latRad);
}

QPointF latLngToPixel(double lat, double lng, int zoom,
                      double centerLat, double centerLng,
                      int widgetWidth, int widgetHeight)
{
    double n = 1 << zoom;

    // World pixel X (0 to n*256)
    double worldX = ((lng + 180.0) / 360.0) * n * TILE_SIZE;

    // World pixel Y (0 to n*256)
    double tileY = latToTileY(lat, zoom);
    double worldY = tileY * TILE_SIZE;

    // Center world pixel
    double centerWorldX = ((centerLng + 180.0) / 360.0) * n * TILE_SIZE;
    double centerTileY = latToTileY(centerLat, zoom);
    double centerWorldY = centerTileY * TILE_SIZE;

    // Widget pixel = world pixel - center world pixel + widget center
    double widgetX = worldX - centerWorldX + widgetWidth / 2.0;
    double widgetY = worldY - centerWorldY + widgetHeight / 2.0;

    return QPointF(widgetX, widgetY);
}

QPointF latLngToPixel(double lat, double lng, double zoom,
                      double centerLat, double centerLng,
                      int widgetWidth, int widgetHeight)
{
    double n = qPow(2.0, zoom);

    // World pixel X (0 to n*256)
    double worldX = ((lng + 180.0) / 360.0) * n * TILE_SIZE;

    // World pixel Y (0 to n*256)
    double tileY = latToTileY(lat, zoom);
    double worldY = tileY * TILE_SIZE;

    // Center world pixel
    double centerWorldX = ((centerLng + 180.0) / 360.0) * n * TILE_SIZE;
    double centerTileY = latToTileY(centerLat, zoom);
    double centerWorldY = centerTileY * TILE_SIZE;

    // Widget pixel = world pixel - center world pixel + widget center
    double widgetX = worldX - centerWorldX + widgetWidth / 2.0;
    double widgetY = worldY - centerWorldY + widgetHeight / 2.0;

    return QPointF(widgetX, widgetY);
}

QPointF pixelToLatLng(const QPointF& pixel, int zoom,
                      double centerLat, double centerLng,
                      int widgetWidth, int widgetHeight)
{
    double n = 1 << zoom;

    // Center world pixel
    double centerWorldX = ((centerLng + 180.0) / 360.0) * n * TILE_SIZE;
    double centerTileY = latToTileY(centerLat, zoom);
    double centerWorldY = centerTileY * TILE_SIZE;

    // World pixel = widget pixel - widget center + center world pixel
    double worldX = pixel.x() - widgetWidth / 2.0 + centerWorldX;
    double worldY = pixel.y() - widgetHeight / 2.0 + centerWorldY;

    // Convert world pixel to lat/lng
    double lng = worldX / (n * TILE_SIZE) * 360.0 - 180.0;
    double tileY = worldY / TILE_SIZE;
    double lat = tileYToLat(tileY, zoom);

    return QPointF(lat, lng);  // Note: returns (lat, lng), not (x, y)
}

QPointF pixelToLatLng(const QPointF& pixel, double zoom,
                      double centerLat, double centerLng,
                      int widgetWidth, int widgetHeight)
{
    double n = qPow(2.0, zoom);

    // Center world pixel
    double centerWorldX = ((centerLng + 180.0) / 360.0) * n * TILE_SIZE;
    double centerTileY = latToTileY(centerLat, zoom);
    double centerWorldY = centerTileY * TILE_SIZE;

    // World pixel = widget pixel - widget center + center world pixel
    double worldX = pixel.x() - widgetWidth / 2.0 + centerWorldX;
    double worldY = pixel.y() - widgetHeight / 2.0 + centerWorldY;

    // Convert world pixel to lat/lng
    double lng = worldX / (n * TILE_SIZE) * 360.0 - 180.0;
    double tileY = worldY / TILE_SIZE;
    double lat = tileYToLat(tileY, zoom);

    return QPointF(lat, lng);  // Note: returns (lat, lng), not (x, y)
}

double lngDegreesPerPixel(int zoom)
{
    double n = 1 << zoom;
    return 360.0 / (n * TILE_SIZE);
}

double lngDegreesPerPixel(double zoom)
{
    double n = qPow(2.0, zoom);
    return 360.0 / (n * TILE_SIZE);
}

double latDegreesPerPixel(int zoom, double atLat)
{
    return lngDegreesPerPixel(zoom) * qCos(qDegreesToRadians(atLat));
}

double latDegreesPerPixel(double zoom, double atLat)
{
    return lngDegreesPerPixel(zoom) * qCos(qDegreesToRadians(atLat));
}

}  // namespace SlippyMapMath
