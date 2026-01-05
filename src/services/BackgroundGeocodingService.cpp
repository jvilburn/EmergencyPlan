#include "BackgroundGeocodingService.h"

BackgroundGeocodingService::BackgroundGeocodingService(QObject* parent)
    : QObject(parent)
    , m_geocodingService(new GeocodingService(this))
{
    connect(m_geocodingService, &GeocodingService::geocodingComplete,
            this, &BackgroundGeocodingService::onGeocodingComplete);
}

BackgroundGeocodingService::~BackgroundGeocodingService() = default;

void BackgroundGeocodingService::queueFamily(const Family& family)
{
    // Skip if already queued
    if (m_queuedIds.contains(family.id()))
    {
        return;
    }

    QString address = family.address().full();
    if (address.trimmed().isEmpty())
    {
        return;
    }

    m_queuedIds.insert(family.id());
    m_addressToFamilyId.insert(address, family.id());
    m_total++;

    emit progressUpdated(m_completed, m_total);

    m_geocodingService->geocodeAddress(address);
}

void BackgroundGeocodingService::stop()
{
    // Clear pending state
    m_addressToFamilyId.clear();
    m_queuedIds.clear();
    m_completed = 0;
    m_total = 0;

    // Note: GeocodingService will complete its current request,
    // but we won't process the result since the maps are cleared
}

bool BackgroundGeocodingService::isRunning() const
{
    return m_geocodingService->hasPendingRequests()
           || !m_addressToFamilyId.isEmpty();
}

void BackgroundGeocodingService::onGeocodingComplete(const GeocodingResult& result)
{
    // Find the family ID for this address
    QString familyId = m_addressToFamilyId.take(result.address);
    if (familyId.isEmpty())
    {
        // Address not in our map - was stopped or duplicate
        return;
    }

    m_queuedIds.remove(familyId);
    m_completed++;

    if (result.success && result.latitude.has_value() && result.longitude.has_value())
    {
        emit familyGeocoded(familyId, *result.latitude, *result.longitude);
    }

    emit progressUpdated(m_completed, m_total);

    // Check if all done
    if (m_addressToFamilyId.isEmpty() && !m_geocodingService->hasPendingRequests())
    {
        emit finished();
        // Reset counters for next batch
        m_completed = 0;
        m_total = 0;
    }
}
