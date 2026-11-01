#include "AutomaticUpdates.h"
#include "Geocoder.h"
#include "UnitLookup.h"

#include <QDebug>
#include <QStringList>
#include <algorithm>

namespace
{

/// The document's wards in order of unit number.
QList<Ward> wardsByUnit(const Document& document)
{
    QList<Ward> wards = document.wards().values();
    std::sort(wards.begin(), wards.end(), [](const Ward& a, const Ward& b) { return a.unitNumber() < b.unitNumber(); });
    return wards;
}

/// ward with each detail the lookup found set over it, keeping what the lookup lacks.
Ward withLookedUpDetails(const Ward& ward, const Ward& found)
{
    Ward updated = ward;
    if (found.chapelAddress().has_value())
    {
        updated.setChapelAddress(found.chapelAddress());
    }
    if (found.chapelLat().has_value())
    {
        updated.setChapelLat(found.chapelLat());
    }
    if (found.chapelLng().has_value())
    {
        updated.setChapelLng(found.chapelLng());
    }
    if (found.chapelPhone().has_value())
    {
        updated.setChapelPhone(found.chapelPhone());
    }
    if (found.meetingTime().has_value())
    {
        updated.setMeetingTime(found.meetingTime());
    }
    if (!found.stakeUnitNumber().isEmpty())
    {
        updated.setStakeUnitNumber(found.stakeUnitNumber());
    }
    return updated;
}

}  // namespace

AutomaticUpdates::AutomaticUpdates(Document& document, Geocoder* geocoder, UnitLookup* unitLookup,
                                   QObject* parent)
    : QObject(parent)
    , m_document(document)
    , m_geocoder(geocoder)
    , m_unitLookup(unitLookup)
{
    connect(m_geocoder, &Geocoder::resolved, this, &AutomaticUpdates::onGeocoded);
    connect(m_geocoder, &Geocoder::failed, this, &AutomaticUpdates::onGeocodeFailed);
    connect(m_unitLookup, &UnitLookup::wardFound, this, &AutomaticUpdates::onWardFound);
    connect(m_unitLookup, &UnitLookup::stakeFound, this, &AutomaticUpdates::onStakeFound);
    connect(m_unitLookup, &UnitLookup::lookupFailed, this, &AutomaticUpdates::onLookupFailed);
}

void AutomaticUpdates::wardOpened()
{
    m_failed.clear();
}

void AutomaticUpdates::commandApplied(const Command& command)
{
    bool workGiven = false;
    for (const Address& address : command.addressesGiven())
    {
        workGiven = workGiven || needsCoordinates(address);
    }
    for (const QString& unit : command.unitsGiven())
    {
        workGiven = workGiven || unitNeedsLookup(unit);
    }
    if (workGiven)
    {
        run();
    }
}

void AutomaticUpdates::run()
{
    QSet<QString> requests;
    for (const Family& family : m_document.families())
    {
        if (needsCoordinates(family.address()))
        {
            requests.insert(family.address().full());
        }
    }

    if (!requests.isEmpty())
    {
        QStringList ordered(requests.cbegin(), requests.cend());
        ordered.sort();
        m_pending.unite(requests);
        m_batchTotal += ordered.size();
        emit geocodingProgress(m_batchTotal - m_pending.size(), m_batchTotal);
        for (const QString& address : ordered)
        {
            m_geocoder->geocode(address);
        }
    }
    else if (m_pending.isEmpty())
    {
        applyBatch();
    }

    lookUpNextUnit();
}

bool AutomaticUpdates::needsCoordinates(const Address& address) const
{
    QString text = address.full();
    return !address.noStreetAddress()
        && !m_document.addressCoordinates().contains(text)
        && !m_failed.contains(text)
        && !m_pending.contains(text)
        && !m_resolved.contains(text);
}

void AutomaticUpdates::onGeocoded(const QString& address, const Coordinates& coordinates)
{
    if (!m_pending.remove(address))
    {
        return;
    }
    m_resolved.insert(address, coordinates);

    // The families at this address are mapped now, before the batch finishes.
    if (!m_document.addressCoordinates().contains(address))
    {
        QList<FamilyId> placed;
        for (const Family& family : m_document.families())
        {
            if (family.address().full() == address)
            {
                placed.append(family.id());
            }
        }
        if (!placed.isEmpty())
        {
            m_document.setAddressCoordinates(address, coordinates);
            m_batchChanged = true;
            for (const FamilyId& id : placed)
            {
                emit documentChanged(DocumentChange::family().updated(id));
            }
        }
    }
    onBatchRequestAnswered();
}

void AutomaticUpdates::onGeocodeFailed(const QString& address)
{
    if (!m_pending.remove(address))
    {
        return;
    }
    m_failed.insert(address);
    onBatchRequestAnswered();
}

void AutomaticUpdates::onBatchRequestAnswered()
{
    emit geocodingProgress(m_batchTotal - m_pending.size(), m_batchTotal);
    if (m_pending.isEmpty())
    {
        applyBatch();
    }
}

void AutomaticUpdates::applyBatch()
{
    QHash<QString, Coordinates> before = m_document.addressCoordinates();
    for (auto it = m_resolved.cbegin(); it != m_resolved.cend(); ++it)
    {
        if (!m_document.addressCoordinates().contains(it.key()))
        {
            m_document.setAddressCoordinates(it.key(), it.value());
        }
    }

    QSet<QString> used;
    for (const Family& family : m_document.families())
    {
        used.insert(family.address().full());
    }
    for (const QString& address : m_document.addressCoordinates().keys())
    {
        if (!used.contains(address))
        {
            m_document.setAddressCoordinates(address, std::nullopt);
        }
    }

    bool requestedAny = m_batchTotal > 0;
    bool changedNow = m_document.addressCoordinates() != before;
    bool changedInBatch = m_batchChanged || changedNow;
    m_resolved.clear();
    m_batchTotal = 0;
    m_batchChanged = false;
    if (requestedAny)
    {
        emit geocodingFinished();
    }
    if (changedNow)
    {
        emit documentChanged(DocumentChange::full());
    }
    if (changedInBatch)
    {
        emit applied();
    }
}

void AutomaticUpdates::lookUpNextUnit()
{
    if (!m_unitInFlight.isEmpty())
    {
        return;
    }
    QString stake = nextStakeToLookUp();
    if (!stake.isEmpty())
    {
        m_unitInFlight = stake;
        m_unitLookup->lookupStake(stake);
        return;
    }
    QString ward = nextWardToLookUp();
    if (!ward.isEmpty())
    {
        m_unitInFlight = ward;
        m_unitLookup->lookupWard(ward);
    }
}

QString AutomaticUpdates::nextStakeToLookUp() const
{
    for (const Ward& ward : wardsByUnit(m_document))
    {
        if (stakeNeedsDetails(ward.stakeUnitNumber()))
        {
            return ward.stakeUnitNumber();
        }
    }
    return QString();
}

QString AutomaticUpdates::nextWardToLookUp() const
{
    for (const Ward& ward : wardsByUnit(m_document))
    {
        if (wardNeedsDetails(ward))
        {
            return ward.unitNumber();
        }
    }
    return QString();
}

bool AutomaticUpdates::wardNeedsDetails(const Ward& ward) const
{
    return !ward.unitNumber().isEmpty()
        && !ward.chapelAddress().has_value()
        && !m_failed.contains(ward.unitNumber());
}

bool AutomaticUpdates::stakeNeedsDetails(const QString& stakeUnitNumber) const
{
    return !stakeUnitNumber.isEmpty()
        && !m_document.stakes().contains(stakeUnitNumber)
        && !m_failed.contains(stakeUnitNumber);
}

bool AutomaticUpdates::unitNeedsLookup(const QString& wardUnitNumber) const
{
    std::optional<Ward> ward = m_document.metadata().findWardByUnit(wardUnitNumber);
    return ward.has_value()
        && (wardNeedsDetails(*ward) || stakeNeedsDetails(ward->stakeUnitNumber()));
}

void AutomaticUpdates::onWardFound(const QString& wardUnitNumber, const Ward& ward)
{
    if (wardUnitNumber != m_unitInFlight)
    {
        return;
    }
    m_unitInFlight.clear();

    std::optional<Ward> existing = m_document.metadata().findWardByUnit(wardUnitNumber);
    if (existing.has_value())
    {
        Ward updated = withLookedUpDetails(*existing, ward);
        if (updated != *existing)
        {
            m_document.metadata().updateWard(updated);
            emit documentChanged(DocumentChange::metadata().updated());
            emit applied();
        }
        // A lookup that found no chapel did not resolve the unit.
        if (!updated.chapelAddress().has_value())
        {
            m_failed.insert(wardUnitNumber);
        }
    }
    lookUpNextUnit();
}

void AutomaticUpdates::onStakeFound(const QString& stakeUnitNumber, const Stake& stake)
{
    if (stakeUnitNumber != m_unitInFlight)
    {
        return;
    }
    m_unitInFlight.clear();

    if (m_document.metadata().findStakeByUnit(stakeUnitNumber) != stake)
    {
        m_document.metadata().updateStake(stake);
        emit documentChanged(DocumentChange::metadata().updated());
        emit applied();
    }
    lookUpNextUnit();
}

void AutomaticUpdates::onLookupFailed(const QString& unitNumber, const QString& error)
{
    if (unitNumber != m_unitInFlight)
    {
        return;
    }
    m_unitInFlight.clear();
    qWarning() << "Lookup failed for" << unitNumber << ":" << error;
    m_failed.insert(unitNumber);
    lookUpNextUnit();
}
