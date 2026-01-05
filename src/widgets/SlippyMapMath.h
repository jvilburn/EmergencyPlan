#pragma once

#include <QPointF>

/// Pure math utilities for slippy map coordinate conversion.
/// Based on OpenStreetMap tile numbering scheme.
namespace SlippyMapMath
{
    constexpr int TILE_SIZE = 256;

    /// Convert latitude to tile Y coordinate
    double latToTileY(double lat, int zoom);
    double latToTileY(double lat, double zoom);

    /// Convert tile Y coordinate to latitude
    double tileYToLat(double tileY, int zoom);
    double tileYToLat(double tileY, double zoom);

    /// Convert lat/lng to widget pixel coordinates
    QPointF latLngToPixel(double lat, double lng, int zoom,
                          double centerLat, double centerLng,
                          int widgetWidth, int widgetHeight);
    QPointF latLngToPixel(double lat, double lng, double zoom,
                          double centerLat, double centerLng,
                          int widgetWidth, int widgetHeight);

    /// Convert widget pixel coordinates to lat/lng
    /// Returns QPointF(lat, lng) - note: lat is x, lng is y
    QPointF pixelToLatLng(const QPointF& pixel, int zoom,
                          double centerLat, double centerLng,
                          int widgetWidth, int widgetHeight);
    QPointF pixelToLatLng(const QPointF& pixel, double zoom,
                          double centerLat, double centerLng,
                          int widgetWidth, int widgetHeight);

    /// Calculate degrees per pixel at a given latitude and zoom
    double lngDegreesPerPixel(int zoom);
    double lngDegreesPerPixel(double zoom);
    double latDegreesPerPixel(int zoom, double atLat);
    double latDegreesPerPixel(double zoom, double atLat);
}
