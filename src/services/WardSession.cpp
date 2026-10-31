#include "WardSession.h"
#include "CommandRegistry.h"
#include "Geocoder.h"
#include "UnitLookup.h"

#include <QDebug>
#include <QStringList>
#include <algorithm>

namespace
{

/// The command's rule, then execute, on document: the one rule a first run and a replay
/// both follow.
CommitResult attempt(const Command& command, Document& document)
{
    QString refusal = command.refusal(document);
    if (!refusal.isEmpty())
    {
        return CommitResult{CommitStatus::Refused, refusal, command.description()};
    }
    if (command.execute(document) == CommandOutcome::Conflicted)
    {
        return CommitResult{CommitStatus::Conflicted, QString(), command.description()};
    }
    document.onDocumentChanged(command.documentChange());
    return CommitResult{CommitStatus::Applied, QString(), command.description()};
}

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

WardSession::WardSession(Geocoder* geocoder, UnitLookup* unitLookup, QObject* parent)
    : QObject(parent)
    , m_geocoder(geocoder)
    , m_unitLookup(unitLookup)
{
    connect(m_geocoder, &Geocoder::resolved, this, &WardSession::onGeocoded);
    connect(m_geocoder, &Geocoder::failed, this, &WardSession::onGeocodeFailed);
    connect(m_unitLookup, &UnitLookup::wardFound, this, &WardSession::onWardFound);
    connect(m_unitLookup, &UnitLookup::stakeFound, this, &WardSession::onStakeFound);
    connect(m_unitLookup, &UnitLookup::lookupFailed, this, &WardSession::onLookupFailed);
}

void WardSession::open(const Document& document)
{
    m_journal = Journal();
    clearUndoStack();
    replaceDocument(document);
}

CommitResult WardSession::commit(CommandPtr command)
{
    if (!command)
    {
        return CommitResult{CommitStatus::NothingToDo, QString(), CommandDescription()};
    }
    CommitResult result = run(*command);
    if (result.status != CommitStatus::Applied)
    {
        return result;
    }

    m_redoStack.clear();
    UndoableCommandPtr undoable = std::dynamic_pointer_cast<const UndoableCommand>(command);
    if (undoable)
    {
        m_undoStack.append(undoable);
        if (m_undoStack.size() > UndoLimit)
        {
            m_undoStack.removeFirst();
        }
    }
    emit undoStackChanged();
    return result;
}

CommitResult WardSession::undo()
{
    Q_ASSERT(canUndo());
    CommitResult result = run(*m_undoStack.last()->inverse());
    if (result.status == CommitStatus::Applied)
    {
        m_redoStack.append(m_undoStack.takeLast());
        emit undoStackChanged();
    }
    return result;
}

CommitResult WardSession::redo()
{
    Q_ASSERT(canRedo());
    CommitResult result = run(*m_redoStack.last());
    if (result.status == CommitStatus::Applied)
    {
        m_undoStack.append(m_redoStack.takeLast());
        emit undoStackChanged();
    }
    return result;
}

QString WardSession::undoHeading() const
{
    if (m_undoStack.isEmpty())
    {
        return QString();
    }
    return m_undoStack.last()->description().heading();
}

QString WardSession::redoHeading() const
{
    if (m_redoStack.isEmpty())
    {
        return QString();
    }
    return m_redoStack.last()->description().heading();
}

ReplayResult WardSession::replayOnto(const Document& version)
{
    if (m_journal.isEmpty())
    {
        replaceDocument(version);
        return ReplayResult{ReplayStatus::FastForwarded, QString()};
    }

    Document replayed = version;
    replayed.onDocumentChanged(DocumentChange::full());
    const QList<JournalEntry>& entries = m_journal.entries();
    for (int i = 0; i < entries.size(); ++i)
    {
        CommandPtr command = CommandRegistry::fromJson(entries[i].command);
        CommitResult result = attempt(*command, replayed);
        if (result.status != CommitStatus::Applied)
        {
            QString toRedo = toRedoText(i, result);
            m_journal.clear();
            clearUndoStack();
            replaceDocument(version);
            return ReplayResult{ReplayStatus::Abandoned, toRedo};
        }
    }
    replaceDocument(replayed);
    return ReplayResult{ReplayStatus::Replayed, QString()};
}

CommitResult WardSession::run(const Command& command)
{
    CommitResult result = attempt(command, m_document);
    if (result.status != CommitStatus::Applied)
    {
        return result;
    }
    m_journal.append(command);
    emit documentChanged(command.documentChange());

    for (const Address& address : command.addressesGiven())
    {
        if (needsCoordinates(address))
        {
            runAutomaticUpdates();
            break;
        }
    }
    return result;
}

QString WardSession::toRedoText(int stopped, const CommitResult& stop) const
{
    QStringList text;
    if (stop.status == CommitStatus::Refused)
    {
        text.append(stop.refusal);
    }
    else
    {
        text.append(tr("Your changes could not be applied, because the ward changed first."));
    }
    text.append(tr("These need doing again:"));

    QList<CommandDescription> descriptions = m_journal.descriptions();
    for (int i = 0; i < descriptions.size(); ++i)
    {
        text.append(QString());
        if (i == stopped)
        {
            text.append(tr("%1 (stopped here)").arg(descriptions[i].heading()));
        }
        else
        {
            text.append(descriptions[i].heading());
        }
        for (const QString& line : descriptions[i].lines())
        {
            text.append(QString("- %1").arg(line));
        }
    }
    return text.join("\n") + "\n";
}

void WardSession::replaceDocument(const Document& document)
{
    m_document = document;
    m_document.onDocumentChanged(DocumentChange::full());
    emit documentChanged(DocumentChange::full());
}

void WardSession::clearUndoStack()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoStackChanged();
}

void WardSession::runAutomaticUpdates()
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

bool WardSession::needsCoordinates(const Address& address) const
{
    QString text = address.full();
    return !address.noStreetAddress()
        && !m_document.addressCoordinates().contains(text)
        && !m_failed.contains(text)
        && !m_pending.contains(text)
        && !m_resolved.contains(text);
}

void WardSession::onGeocoded(const QString& address, const Coordinates& coordinates)
{
    if (!m_pending.remove(address))
    {
        return;
    }
    m_resolved.insert(address, coordinates);
    onBatchRequestAnswered();
}

void WardSession::onGeocodeFailed(const QString& address)
{
    if (!m_pending.remove(address))
    {
        return;
    }
    m_failed.insert(address);
    onBatchRequestAnswered();
}

void WardSession::onBatchRequestAnswered()
{
    emit geocodingProgress(m_batchTotal - m_pending.size(), m_batchTotal);
    if (m_pending.isEmpty())
    {
        applyBatch();
    }
}

void WardSession::applyBatch()
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
    m_resolved.clear();
    m_batchTotal = 0;
    if (requestedAny)
    {
        emit geocodingFinished();
    }
    if (m_document.addressCoordinates() != before)
    {
        emit documentChanged(DocumentChange::full());
        emit automaticUpdateApplied();
    }
}

void WardSession::lookUpNextUnit()
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

QString WardSession::nextStakeToLookUp() const
{
    for (const Ward& ward : wardsByUnit(m_document))
    {
        QString stake = ward.stakeUnitNumber();
        if (!stake.isEmpty()
            && !m_document.stakes().contains(stake)
            && !m_failed.contains(stake))
        {
            return stake;
        }
    }
    return QString();
}

QString WardSession::nextWardToLookUp() const
{
    for (const Ward& ward : wardsByUnit(m_document))
    {
        if (!ward.unitNumber().isEmpty()
            && !ward.chapelAddress().has_value()
            && !m_failed.contains(ward.unitNumber()))
        {
            return ward.unitNumber();
        }
    }
    return QString();
}

void WardSession::onWardFound(const QString& wardUnitNumber, const Ward& ward)
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
            emit automaticUpdateApplied();
        }
        // A lookup that found no chapel did not resolve the unit.
        if (!updated.chapelAddress().has_value())
        {
            m_failed.insert(wardUnitNumber);
        }
    }
    lookUpNextUnit();
}

void WardSession::onStakeFound(const QString& stakeUnitNumber, const Stake& stake)
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
        emit automaticUpdateApplied();
    }
    lookUpNextUnit();
}

void WardSession::onLookupFailed(const QString& unitNumber, const QString& error)
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
