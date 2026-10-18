#include "DocumentManager.h"
#include "UnitLookupService.h"
#include "BackgroundGeocodingService.h"

#include <QDebug>

DocumentManager* DocumentManager::s_instance = nullptr;

DocumentManager::DocumentManager(QObject* parent)
    : QObject(parent)
    , m_session(this)
    , m_unitLookupService(new UnitLookupService(this))
    , m_geocodingService(nullptr)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_session.open(Document::empty());

    // Create after s_instance is set so BackgroundGeocodingService can use instance()
    m_geocodingService = new BackgroundGeocodingService(this);

    connect(&m_session, &WardSession::documentChanged, this, &DocumentManager::documentChanged);
    connect(&m_session, &WardSession::undoStackChanged, this, &DocumentManager::undoStackChanged);

    connect(m_unitLookupService, &UnitLookupService::wardLookupComplete,
            this, &DocumentManager::onWardLookupComplete);
    connect(m_unitLookupService, &UnitLookupService::stakeLookupComplete,
            this, &DocumentManager::onStakeLookupComplete);
    connect(m_unitLookupService, &UnitLookupService::lookupFailed,
            this, &DocumentManager::onLookupFailed);

    connect(m_geocodingService, &BackgroundGeocodingService::progressUpdated,
            this, &DocumentManager::geocodingProgressChanged);
    connect(m_geocodingService, &BackgroundGeocodingService::familyGeocoded,
            this, &DocumentManager::onFamilyGeocoded);
    connect(m_geocodingService, &BackgroundGeocodingService::finished,
            this, &DocumentManager::onGeocodingFinished);
}

DocumentManager* DocumentManager::instance()
{
    return s_instance;
}

CommitResult DocumentManager::executeCommand(CommandPtr command)
{
    CommitResult result = m_session.commit(command);
    if (result.status == CommitStatus::NothingToDo)
    {
        return result;
    }
    afterCommit(result);
    return result;
}

void DocumentManager::undo()
{
    if (!canUndo())
    {
        return;
    }
    afterCommit(m_session.undo());
}

void DocumentManager::redo()
{
    if (!canRedo())
    {
        return;
    }
    afterCommit(m_session.redo());
}

void DocumentManager::afterCommit(const CommitResult& result)
{
    switch (result.status)
    {
    case CommitStatus::Applied:
        checkForIncompleteWards();
        break;
    case CommitStatus::Conflicted:
    case CommitStatus::Refused:
        emit commandNotApplied(result);
        break;
    case CommitStatus::NothingToDo:
        break;
    }
}

void DocumentManager::newDocument()
{
    stopGeocoding();
    m_pendingWardLookups.clear();
    m_pendingStakeLookups.clear();
    m_session.open(Document::empty());
}

void DocumentManager::checkForIncompleteWards()
{
    // Find wards that need lookup (no chapel address means incomplete)
    for (const Ward& ward : m_session.document().wards())
    {
        QString unitNumber = ward.unitNumber();
        if (!ward.chapelAddress().has_value()
            && !m_pendingWardLookups.contains(unitNumber))
        {
            m_pendingWardLookups.insert(unitNumber);
            m_unitLookupService->lookupWard(unitNumber);
            return;  // One at a time to avoid overloading the API
        }
    }
}

void DocumentManager::onWardLookupComplete(const QString& wardUnitNumber, const Ward& wardInfo)
{
    m_pendingWardLookups.remove(wardUnitNumber);

    // Update ward with chapel info
    std::optional<Ward> existing = m_document.metadata().findWardByUnit(wardUnitNumber);
    if (existing.has_value())
    {
        Ward updated = *existing;

        // Merge in the lookup results (don't overwrite existing data)
        if (wardInfo.chapelAddress().has_value())
        {
            updated.setChapelAddress(wardInfo.chapelAddress());
        }
        if (wardInfo.chapelLat().has_value())
        {
            updated.setChapelLat(wardInfo.chapelLat());
        }
        if (wardInfo.chapelLng().has_value())
        {
            updated.setChapelLng(wardInfo.chapelLng());
        }
        if (wardInfo.chapelPhone().has_value())
        {
            updated.setChapelPhone(wardInfo.chapelPhone());
        }
        if (wardInfo.meetingTime().has_value())
        {
            updated.setMeetingTime(wardInfo.meetingTime());
        }
        if (!wardInfo.stakeUnitNumber().isEmpty())
        {
            updated.setStakeUnitNumber(wardInfo.stakeUnitNumber());
        }

        m_document.metadata().updateWard(updated);
        emit documentChanged(DocumentChange::metadata().updated(wardUnitNumber));

        // Trigger stake lookup if we got stake unit number and don't already have it
        QString stakeUnit = wardInfo.stakeUnitNumber();
        if (!stakeUnit.isEmpty()
            && !m_pendingStakeLookups.contains(stakeUnit)
            && !m_document.metadata().findStakeByUnit(stakeUnit).has_value())
        {
            m_pendingStakeLookups.insert(stakeUnit);
            m_unitLookupService->lookupStake(stakeUnit);
        }
    }

    // Process next incomplete ward
    checkForIncompleteWards();
}

void DocumentManager::onStakeLookupComplete(const QString& stakeUnitNumber, const Stake& stakeInfo)
{
    m_pendingStakeLookups.remove(stakeUnitNumber);

    // Add or update stake
    m_document.metadata().updateStake(stakeInfo);
    emit documentChanged(DocumentChange::metadata().updated(stakeUnitNumber));
}

void DocumentManager::onLookupFailed(const QString& unitNumber, const QString& error)
{
    qWarning() << "Lookup failed for" << unitNumber << ":" << error;
    m_pendingWardLookups.remove(unitNumber);
    m_pendingStakeLookups.remove(unitNumber);
    // No retry here: the next applied change looks up incomplete wards again
}

// ============================================================================
// Geocoding
// ============================================================================

void DocumentManager::startBatchGeocoding()
{
    // Queue all families that need geocoding
    for (const auto& family : m_session.document().families())
    {
        if (!family.address().isEmpty() && !family.isMapped())
        {
            m_geocodingService->queueFamily(family);
        }
    }
}

void DocumentManager::stopGeocoding()
{
    m_geocodingService->stop();
}

void DocumentManager::onFamilyGeocoded(const FamilyId& id, double latitude, double longitude)
{
    // Silent update - no undo/redo, no signal (nothing needs to rebuild for coord changes)
    auto existing = m_document.findFamilyById(id);
    if (existing.has_value())
    {
        Family updated = *existing;
        updated.setLocation(latitude, longitude);
        m_document.updateFamily(updated);
    }
}

void DocumentManager::onGeocodingFinished()
{
    emit geocodingFinished();
}
