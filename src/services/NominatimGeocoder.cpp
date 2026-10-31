#include "NominatimGeocoder.h"
#include "GeocodingService.h"

NominatimGeocoder::NominatimGeocoder(QObject* parent)
    : Geocoder(parent)
    , m_service(new GeocodingService(this))
{
    connect(m_service, &GeocodingService::geocodingComplete, this, &NominatimGeocoder::onGeocodingComplete);
}

void NominatimGeocoder::geocode(const QString& address)
{
    m_service->geocodeAddress(address);
}

void NominatimGeocoder::onGeocodingComplete(const GeocodingResult& result)
{
    if (result.success && result.latitude.has_value() && result.longitude.has_value())
    {
        emit resolved(result.address, Coordinates(*result.latitude, *result.longitude));
    }
    else
    {
        emit failed(result.address);
    }
}
