#include "BackgroundGeocodingService.h"
#include "DocumentManager.h"

BackgroundGeocodingService::BackgroundGeocodingService(QObject* parent)
    : QObject(parent)
    , m_geocodingService(new GeocodingService(this))
{
    connect(m_geocodingService, &GeocodingService::geocodingComplete,
            this, &BackgroundGeocodingService::onGeocodingComplete);

    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &BackgroundGeocodingService::onDocumentChanged);
}

BackgroundGeocodingService::~BackgroundGeocodingService() = default;

void BackgroundGeocodingService::queueFamily(const Family& family)
{
    // Skip if already queued
    if (m_queuedIds.contains(family.id()))
    {
        return;
    }

    // Skip addresses without a street (e.g., only "Utah" or "Provo, UT")
    if (family.address().noStreetAddress())
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
    auto it = m_addressToFamilyId.find(result.address);
    if (it == m_addressToFamilyId.end())
    {
        // Address not in our map - was stopped or duplicate
        return;
    }
    FamilyId familyId = *it;
    m_addressToFamilyId.erase(it);

    m_queuedIds.remove(familyId);
    m_completed++;

    if (result.success && result.latitude.has_value() && result.longitude.has_value())
    {
        // Add to cache for future lookups
        m_geocodeCache.insert(result.address,
            QPointF(*result.latitude, *result.longitude));

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

void BackgroundGeocodingService::onDocumentChanged(const DocumentChange& change)
{
    // Full document change or batch family change - process all families
    if (change.action == ChangeAction::Full)
    {
        processAllFamilies();
        return;
    }

    // Single family change - check that family
    if (change.familyId.has_value())
    {
        auto family = DocumentManager::instance()->document().findFamilyById(*change.familyId);
        if (family.has_value())
        {
            checkFamily(*family);
        }
    }
}

void BackgroundGeocodingService::checkFamily(const Family& family)
{
    QString address = family.address().full();
    QString lastAddress = m_familyAddresses.value(family.id());

    bool addressChanged = (address != lastAddress);

    // Update address tracking
    if (!address.isEmpty())
    {
        m_familyAddresses.insert(family.id(), address);
    }
    else
    {
        m_familyAddresses.remove(family.id());
    }

    if (address.isEmpty())
    {
        return;
    }

    if (addressChanged)
    {
        // Address changed - existing coords are stale, need fresh geocoding
        if (m_geocodeCache.contains(address))
        {
            QPointF cached = m_geocodeCache.value(address);
            emit familyGeocoded(family.id(), cached.x(), cached.y());
        }
        else
        {
            queueFamily(family);
        }
        return;
    }

    // Address unchanged
    if (family.isMapped())
    {
        // Has coords for current address - trust them (user correction or already geocoded)
        return;
    }

    // Unmapped - apply cache if available, otherwise queue
    if (m_geocodeCache.contains(address))
    {
        QPointF cached = m_geocodeCache.value(address);
        emit familyGeocoded(family.id(), cached.x(), cached.y());
    }
    else
    {
        queueFamily(family);
    }
}

void BackgroundGeocodingService::processAllFamilies()
{
    m_geocodeCache.clear();
    m_familyAddresses.clear();
    const auto& families = DocumentManager::instance()->document().families();

    // First pass: seed caches from document state.
    // - Address tracking: so we can detect changes later
    // - Coord cache: so families at same address can share coords
    for (const auto& family : families)
    {
        QString address = family.address().full();

        if (!address.isEmpty())
        {
            m_familyAddresses.insert(family.id(), address);

            if (family.isMapped())
            {
                m_geocodeCache.insert(address,
                    QPointF(family.latitude().value(), family.longitude().value()));
            }
        }
    }

    // Second pass: check each family against the now-seeded cache.
    // Since addresses are already tracked, checkFamily will see addressChanged=false,
    // trusting mapped families and only geocoding unmapped ones.
    for (const auto& family : families)
    {
        checkFamily(family);
    }
}
