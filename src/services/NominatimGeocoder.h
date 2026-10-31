#pragma once

#include "Geocoder.h"

class GeocodingService;
struct GeocodingResult;

/// The app's geocoder, over OpenStreetMap's Nominatim through GeocodingService.
class NominatimGeocoder : public Geocoder
{
    Q_OBJECT

public:
    explicit NominatimGeocoder(QObject* parent);

    void geocode(const QString& address) override;

private slots:
    void onGeocodingComplete(const GeocodingResult& result);

private:
    GeocodingService* m_service;
};
