#include "DocumentManager.h"
#include "BackgroundGeocodingService.h"
#include "DocumentCrypto.h"
#include "RosterDiff.h"
#include "ServerVersion.h"
#include "ServerConnection.h"
#include "UnitLookupService.h"

#include <QDebug>
#include <QStandardPaths>

using namespace std::placeholders;

DocumentManager* DocumentManager::s_instance = nullptr;

DocumentManager::DocumentManager(const DeviceIdentity& identity,
                                 DocumentRecords& records,
                                 ServerConnection& server,
                                 DocumentLibrary& library,
                                 QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_records(&records)
    , m_server(&server)
    , m_library(&library)
    , m_empty(Document::empty())
    , m_commandHistory(this)
{
    connectServices();
}

DocumentManager::DocumentManager(const DeviceIdentity& identity,
                                 DocumentRecords& records,
                                 std::unique_ptr<DocumentSession> session,
                                 QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_records(&records)
    , m_empty(Document::empty())
    , m_commandHistory(this)
{
    connectServices();
    install(std::move(session));
}

DocumentManager::~DocumentManager()
{
    // The session closes as it goes: a scheduled save is written and the lease
    // released.
    m_session.reset();
    s_instance = nullptr;
}

void DocumentManager::connectServices()
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    // Made after s_instance is set, since both reach the manager through instance().
    m_unitLookupService = new UnitLookupService(this);
    m_geocodingService = new BackgroundGeocodingService(this);

    connect(&m_commandHistory, &CommandHistory::canUndoChanged, this, &DocumentManager::canUndoChanged);
    connect(&m_commandHistory, &CommandHistory::canRedoChanged, this, &DocumentManager::canRedoChanged);

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

const Document& DocumentManager::document() const
{
    if (m_copy.has_value())
    {
        return m_copy.value().document;
    }
    if (m_session)
    {
        return m_session->document();
    }
    return m_empty;
}

QString DocumentManager::documentId() const
{
    return m_session ? m_session->documentId() : QString();
}

// ============================================================================
// Editing
// ============================================================================

bool DocumentManager::executeCommand(CommandPtr command)
{
    DocumentSession* session = sessionForEdit();
    if (session == nullptr)
    {
        return false;
    }

    const DocumentChange change = command->documentChange();
    const DocumentSession::Refusal refusal = session->edit(DocumentSession::Edit::Directory, [&](Document& document)
    {
        m_commandHistory.execute(std::move(command), document);
        document.onDocumentChanged(change);
    });
    if (refusal != DocumentSession::Refusal::None)
    {
        refuse(refusal);
        return false;
    }
    afterChange(change);
    return true;
}

void DocumentManager::undo()
{
    if (!canUndo() || !m_session)
    {
        return;
    }

    DocumentChange change = DocumentChange::full();
    const DocumentSession::Refusal refusal =
        m_session->edit(DocumentSession::Edit::Directory, [&](Document& document)
    {
        change = m_commandHistory.undo(document);
        document.onDocumentChanged(change);
    });
    if (refusal != DocumentSession::Refusal::None)
    {
        refuse(refusal);
        return;
    }
    afterChange(change);
}

void DocumentManager::redo()
{
    if (!canRedo() || !m_session)
    {
        return;
    }

    DocumentChange change = DocumentChange::full();
    const DocumentSession::Refusal refusal =
        m_session->edit(DocumentSession::Edit::Directory, [&](Document& document)
    {
        change = m_commandHistory.redo(document);
        document.onDocumentChanged(change);
    });
    if (refusal != DocumentSession::Refusal::None)
    {
        refuse(refusal);
        return;
    }
    afterChange(change);
}

bool DocumentManager::editResponse(const std::function<void(std::optional<EmergencyResponse>&)>& change)
{
    DocumentSession* session = sessionForEdit();
    if (session == nullptr)
    {
        return false;
    }

    const DocumentSession::Refusal refusal = session->edit(DocumentSession::Edit::Response, [&](Document& document)
    {
        std::optional<EmergencyResponse> response = document.emergencyResponse();
        change(response);
        document.setEmergencyResponse(response);
    });
    if (refusal != DocumentSession::Refusal::None)
    {
        refuse(refusal);
        return false;
    }
    session->setEmergencyActive(session->document().emergencyResponse().has_value());
    return true;
}

DocumentSession* DocumentManager::sessionForEdit()
{
    if (m_copy.has_value())
    {
        emit editRefused(tr("This is a read-only copy. Open the ward itself to make changes."));
        return nullptr;
    }
    if (!m_session)
    {
        QString error;
        if (!newDocument(&error))
        {
            emit saveFailed(error);
            return nullptr;
        }
    }
    return m_session.get();
}

void DocumentManager::refuse(DocumentSession::Refusal refusal)
{
    switch (refusal)
    {
    case DocumentSession::Refusal::None:
        break;
    case DocumentSession::Refusal::DirectoryReadOnly:
        emit editRefused(tr("Another member is editing the directory."));
        break;
    case DocumentSession::Refusal::RequiresUpdate:
        emit editRefused(tr("This ward was changed by a newer version of Ward Planning. Update to make changes."));
        break;
    }
}

void DocumentManager::afterChange(const DocumentChange& change)
{
    emit documentChanged(change);
    if (m_library != nullptr && m_session)
    {
        m_library->noteDocument(m_session->documentId(), m_session->document());
    }
    checkForIncompleteWards();
}

// ============================================================================
// Wards
// ============================================================================

DocumentSession::Context DocumentManager::context() const
{
    return DocumentSession::Context{m_identity, *m_records, *m_server};
}

bool DocumentManager::newDocument(QString* errorMessage)
{
    if (m_library == nullptr || !closeCurrent(errorMessage))
    {
        return false;
    }

    const QString documentId = DocumentLibrary::newDocumentId();
    m_library->add(documentId);
    std::unique_ptr<DocumentSession> session = DocumentSession::create(context(),
                                                                       std::make_unique<QtSessionTimers>(),
                                                                       documentId,
                                                                       m_library->pathsFor(documentId),
                                                                       errorMessage);
    if (!session)
    {
        m_library->forget(documentId);
        resetWard();
        return false;
    }

    m_library->markOpened(documentId);
    install(std::move(session));
    return true;
}

DocumentManager::Open DocumentManager::openDocument(const QString& documentId, QString* refusal)
{
    if (m_session && !m_copy.has_value() && m_session->documentId() == documentId)
    {
        return Open::Opened;
    }
    return open(documentId, refusal, true);
}

DocumentManager::Open DocumentManager::open(const QString& documentId, QString* refusal, bool mayFetch)
{
    if (m_library == nullptr || !m_library->contains(documentId))
    {
        return Open::NotFound;
    }
    if (!closeCurrent(refusal))
    {
        return Open::NotClosed;
    }

    LoadFailure failure = LoadFailure::None;
    std::unique_ptr<DocumentSession> session = DocumentSession::open(context(),
                                                                     std::make_unique<QtSessionTimers>(),
                                                                     documentId,
                                                                     m_library->pathsFor(documentId),
                                                                     failure,
                                                                     refusal);
    if (session)
    {
        m_library->markOpened(documentId);
        install(std::move(session));
        return Open::Opened;
    }

    resetWard();
    const bool fetchable = mayFetch && m_records->record(documentId).baseVersion.has_value();
    switch (failure)
    {
    case LoadFailure::None:
        break;
    case LoadFailure::NeedsNewerVersion:
        return Open::NeedsNewerVersion;
    case LoadFailure::NotFound:
        if (fetchable)
        {
            fetch(documentId);
            return Open::Fetching;
        }
        return Open::NotFound;
    case LoadFailure::Refused:
        if (fetchable)
        {
            fetch(documentId);
            return Open::Fetching;
        }
        return Open::Refused;
    }
    return Open::Refused;
}

void DocumentManager::fetch(const QString& documentId)
{
    m_fetching = documentId;
    m_library->fetchWard(documentId, *m_server, this, std::bind(&DocumentManager::onFetched, this, _1, _2));
}

void DocumentManager::onFetched(const QString& documentId, DocumentSession::Download outcome)
{
    // Another ward was shown meanwhile; this one waits in the library.
    if (m_fetching != documentId)
    {
        return;
    }
    m_fetching.clear();

    switch (outcome)
    {
    case DocumentSession::Download::Done:
    {
        // What was fetched opens directly: a refusal now is reported, not fetched again.
        QString reason;
        const Open result = open(documentId, &reason, false);
        emit fetchFinished(documentId, result, reason);
        return;
    }
    case DocumentSession::Download::AccessLost:
        // Without the ward's roster there is no saying who; the version simply does
        // not seal to this device any more.
        forgetLostWard(documentId, ServerVersion::LostAccess{ServerVersion::LostAccess::How::Removed, QString()});
        return;
    case DocumentSession::Download::Unreachable:
    case DocumentSession::Download::NeedsNewerVersion:
    case DocumentSession::Download::ServerFault:
    case DocumentSession::Download::WriteFailed:
        emit fetchFailed(documentId, outcome);
        return;
    }
}

bool DocumentManager::showCopy(const DocumentLibrary::Copy& copy, QString* errorMessage)
{
    if (!closeCurrent(errorMessage))
    {
        return false;
    }

    m_copy = copy;
    onDocumentReplaced();
    emit documentOpened();
    emit readOnlyChanged();
    return true;
}

std::optional<DocumentSession::Conflict> DocumentManager::pendingConflict() const
{
    return m_session ? m_session->pendingConflict() : std::nullopt;
}

bool DocumentManager::saveRecoveryCopy(QString* path, QString* errorMessage)
{
    if (!m_session || m_library == nullptr)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = tr("No ward is open.");
        }
        return false;
    }
    return m_library->writeRecoveryCopy(m_session->documentId(),
                                        m_session->document(),
                                        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                        path,
                                        errorMessage);
}

bool DocumentManager::resolveConflict(DocumentSession::Resolution resolution, QString* errorMessage)
{
    if (!m_session)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = tr("No ward is open.");
        }
        return false;
    }
    if (!m_session->resolve(resolution, errorMessage))
    {
        return false;
    }
    emit conflictSettled();
    return true;
}

bool DocumentManager::publishNow(QString* errorMessage)
{
    if (!m_session)
    {
        return true;
    }
    if (!m_session->flush(errorMessage))
    {
        return false;
    }
    m_session->publishNow();
    return true;
}

bool DocumentManager::close(QString* errorMessage)
{
    if (!closeCurrent(errorMessage))
    {
        return false;
    }
    resetWard();
    return true;
}

bool DocumentManager::closeCurrent(QString* errorMessage)
{
    // Whatever shows next, a ward still being fetched waits in the library.
    m_fetching.clear();
    m_copy.reset();
    if (!m_session)
    {
        return true;
    }
    if (!m_session->flush(errorMessage))
    {
        return false;
    }

    stopGeocoding();
    m_session.reset();
    return true;
}

void DocumentManager::resetWard()
{
    m_copy.reset();
    m_rosterChanges.clear();
    m_pendingWardLookups.clear();
    m_pendingStakeLookups.clear();
    onDocumentReplaced();
    emit documentOpened();
    emit readOnlyChanged();
}

void DocumentManager::install(std::unique_ptr<DocumentSession> session)
{
    m_copy.reset();
    m_session = std::move(session);
    m_rosterChanges.clear();
    m_pendingWardLookups.clear();
    m_pendingStakeLookups.clear();

    DocumentSession* opened = m_session.get();
    connect(opened, &DocumentSession::documentReplaced, this, &DocumentManager::onDocumentReplaced);
    connect(opened, &DocumentSession::rosterChanged, this, &DocumentManager::refreshRosterChanges);
    connect(opened, &DocumentSession::rosterChanged, this, &DocumentManager::rosterChanged);
    connect(opened, &DocumentSession::leaveFailed, this, &DocumentManager::leaveFailed);
    connect(opened, &DocumentSession::readOnlyChanged, this, &DocumentManager::onReadOnlyChanged);
    connect(opened, &DocumentSession::syncStateChanged, this, &DocumentManager::syncStateChanged);
    connect(opened, &DocumentSession::conflictRaised, this, &DocumentManager::conflictRaised);
    connect(opened, &DocumentSession::requiresUpdateChanged, this, &DocumentManager::requiresUpdateChanged);
    connect(opened, &DocumentSession::headChanged, this, &DocumentManager::headChanged);
    connect(opened, &DocumentSession::shared, this, &DocumentManager::onShared);
    connect(opened, &DocumentSession::shareFailed, this, &DocumentManager::shareFailed);
    connect(opened, &DocumentSession::saveFailed, this, &DocumentManager::saveFailed);
    connect(opened, &DocumentSession::accessLost, this, &DocumentManager::onSessionLost);

    opened->setEmergencyActive(opened->document().emergencyResponse().has_value());
    onDocumentReplaced();
    refreshRosterChanges();
    emit documentOpened();
    emit readOnlyChanged();
    emit syncStateChanged(opened->syncState());
    if (mayWriteDirectory())
    {
        startBatchGeocoding();
    }
}

void DocumentManager::onDocumentReplaced()
{
    // Undo applies to the document it was recorded on, which this no longer is.
    m_commandHistory.clear();
    if (m_session)
    {
        m_session->setEmergencyActive(m_session->document().emergencyResponse().has_value());
    }
    afterChange(DocumentChange::full());
}

void DocumentManager::onReadOnlyChanged()
{
    if (mayWriteDirectory())
    {
        // Taking the lease is when families still lacking coordinates get them.
        startBatchGeocoding();
        checkForIncompleteWards();
    }
    else
    {
        stopGeocoding();
        m_pendingWardLookups.clear();
        m_pendingStakeLookups.clear();
    }
    emit readOnlyChanged();
}

SealingSet DocumentManager::roster() const
{
    return m_session ? m_session->document().sealingSet() : SealingSet();
}

std::optional<Holder> DocumentManager::self() const
{
    return m_session ? m_session->self() : std::nullopt;
}

void DocumentManager::refreshRosterChanges()
{
    QStringList changes;
    if (m_session)
    {
        // Shown for the first time, a ward's roster is what it is; from then on, what
        // changed since the notice was last dismissed.
        DocumentRecord record = m_records->record(m_session->documentId());
        const std::optional<SealingSet> previous = SealingSet::fromJson(record.rosterShown);
        if (!previous.has_value())
        {
            record.rosterShown = roster().toJson();
            m_records->setRecord(m_session->documentId(), record);
        }
        else
        {
            changes = RosterDiff::describe(previous.value(), roster());
        }
    }

    if (changes != m_rosterChanges)
    {
        m_rosterChanges = changes;
        emit accessChanged();
    }
}

void DocumentManager::dismissRosterChanges()
{
    if (!m_session || m_rosterChanges.isEmpty())
    {
        return;
    }

    // Seen for good, on the record, so the next sync shows only what is newer.
    DocumentRecord record = m_records->record(m_session->documentId());
    record.rosterShown = roster().toJson();
    m_records->setRecord(m_session->documentId(), record);
    m_rosterChanges.clear();
    emit accessChanged();
}

void DocumentManager::share()
{
    if (m_session)
    {
        m_session->share();
    }
}

std::optional<DocumentManager::Invitation> DocumentManager::invite(Invite::For purpose, Role role, const QString& label)
{
    const std::optional<Holder> me = self();
    if (!me.has_value() || !m_session->isShared())
    {
        return std::nullopt;
    }

    const FileOpenRouter::InviteLink link{m_session->documentId(),
                                          DocumentCrypto::randomBytes(FileOpenRouter::InviteLink::SeedSize)};
    SealingSet changed = roster();
    if (!changed.addInvite(link.invite(purpose, me->id, role, label)))
    {
        return std::nullopt;
    }
    publishRoster(changed);

    // The message names this person as the roster does, which the recipient will see.
    const QString ward = wardNameOf(link.documentId);
    return Invitation{link, FileOpenRouter::inviteMessage(me->name, ward.isEmpty() ? tr("a ward") : ward, link)};
}

void DocumentManager::publishRoster(const SealingSet& roster)
{
    m_session->edit(DocumentSession::Edit::Roster, std::bind(&Document::setSealingSet, _1, roster));
    m_session->publishNow();
}

void DocumentManager::removePerson(const MemberId& id)
{
    // Leaving is its own call; a roster without this person is never saved here.
    const std::optional<Holder> me = self();
    SealingSet changed = roster();
    if (me.has_value() && me->id != id && changed.removePerson(id))
    {
        publishRoster(changed);
    }
}

void DocumentManager::removeDevice(const Fingerprint& device)
{
    SealingSet changed = roster();
    if (device != m_identity.fingerprint() && changed.removeDevice(device))
    {
        publishRoster(changed);
    }
}

void DocumentManager::withdrawInvite(const Fingerprint& invite)
{
    SealingSet changed = roster();
    if (changed.removeInvite(invite))
    {
        publishRoster(changed);
    }
}

void DocumentManager::setRole(const MemberId& id, Role role)
{
    // A ward always keeps an admin: the last admin makes someone else one first.
    SealingSet changed = roster();
    const std::optional<Holder> person = changed.person(id);
    if (role == Role::Member && person.has_value() && person->isAdmin() && changed.adminCount() == 1)
    {
        return;
    }
    if (changed.setRole(id, role))
    {
        publishRoster(changed);
    }
}

void DocumentManager::leave()
{
    if (m_session)
    {
        m_session->leave();
    }
}

void DocumentManager::joinWard(const FileOpenRouter::InviteLink& link)
{
    if (m_library == nullptr || m_server == nullptr)
    {
        return;
    }
    if (m_library->contains(link.documentId))
    {
        QString reason;
        emit fetchFinished(link.documentId, openDocument(link.documentId, &reason), reason);
        return;
    }
    m_library->join(link, *m_server, this, std::bind(&DocumentManager::onJoined, this, link.documentId, _1, _2));
}

void DocumentManager::onJoined(const QString& documentId, DocumentSession::Join outcome, const QString& reason)
{
    if (outcome != DocumentSession::Join::Done)
    {
        emit joinFailed(outcome, reason);
        return;
    }

    QString refusal;
    const Open result = open(documentId, &refusal, false);
    emit fetchFinished(documentId, result, refusal);
}

void DocumentManager::onShared()
{
    emit shared(m_session->documentId());
}

void DocumentManager::onSessionLost(const ServerVersion::LostAccess& lost)
{
    const QString documentId = m_session->documentId();

    // Finished from inside its own signal, so it goes once that returns.
    stopGeocoding();
    m_session.release()->deleteLater();
    resetWard();
    forgetLostWard(documentId, lost);
}

void DocumentManager::forgetLostWard(const QString& documentId, const ServerVersion::LostAccess& lost)
{
    const QString wardName = wardNameOf(documentId);
    if (m_library != nullptr)
    {
        m_library->forgetLost(documentId);
    }
    emit accessLost(wardName, lost);
}

QString DocumentManager::wardNameOf(const QString& documentId) const
{
    if (m_library == nullptr)
    {
        return QString();
    }
    for (const DocumentLibrary::Entry& entry : m_library->entries())
    {
        if (entry.documentId == documentId)
        {
            return entry.wardName;
        }
    }
    return QString();
}

bool DocumentManager::mayWriteDirectory() const
{
    return m_session && m_session->mayWriteDirectory();
}

// ============================================================================
// Unit lookups
// ============================================================================

void DocumentManager::checkForIncompleteWards()
{
    if (!mayWriteDirectory())
    {
        return;
    }

    // Find wards that need lookup (no chapel address means incomplete)
    for (const Ward& ward : document().wards())
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
    if (!m_pendingWardLookups.remove(wardUnitNumber) || !mayWriteDirectory())
    {
        return;
    }

    std::optional<Ward> existing = document().metadata().findWardByUnit(wardUnitNumber);
    if (!existing.has_value())
    {
        checkForIncompleteWards();
        return;
    }

    // Merge in the lookup results (don't overwrite existing data)
    Ward updated = *existing;
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

    if (m_session->edit(DocumentSession::Edit::Directory, [&](Document& document)
        {
            document.metadata().updateWard(updated);
        })
        != DocumentSession::Refusal::None)
    {
        return;
    }
    emit documentChanged(DocumentChange::metadata().updated(wardUnitNumber));

    // Trigger stake lookup if we got stake unit number and don't already have it
    QString stakeUnit = wardInfo.stakeUnitNumber();
    if (!stakeUnit.isEmpty()
        && !m_pendingStakeLookups.contains(stakeUnit)
        && !document().metadata().findStakeByUnit(stakeUnit).has_value())
    {
        m_pendingStakeLookups.insert(stakeUnit);
        m_unitLookupService->lookupStake(stakeUnit);
    }

    // Process next incomplete ward
    checkForIncompleteWards();
}

void DocumentManager::onStakeLookupComplete(const QString& stakeUnitNumber, const Stake& stakeInfo)
{
    if (!m_pendingStakeLookups.remove(stakeUnitNumber) || !mayWriteDirectory())
    {
        return;
    }

    if (m_session->edit(DocumentSession::Edit::Directory, [&](Document& document)
        {
            document.metadata().updateStake(stakeInfo);
        })
        == DocumentSession::Refusal::None)
    {
        emit documentChanged(DocumentChange::metadata().updated(stakeUnitNumber));
    }
}

void DocumentManager::onLookupFailed(const QString& unitNumber, const QString& error)
{
    qWarning() << "Lookup failed for" << unitNumber << ":" << error;
    m_pendingWardLookups.remove(unitNumber);
    m_pendingStakeLookups.remove(unitNumber);
    // No retry, no checkForIncompleteWards() - will try again on next file open
}

// ============================================================================
// Geocoding
// ============================================================================

void DocumentManager::startBatchGeocoding()
{
    if (!mayWriteDirectory())
    {
        return;
    }

    // Queue all families that need geocoding
    for (const auto& family : document().families())
    {
        if (!family.address().isEmpty() && !family.isMapped())
        {
            m_geocodingService->queueFamily(family);
        }
    }
}

void DocumentManager::stopGeocoding()
{
    const bool wasRunning = m_geocodingService->isRunning();
    m_geocodingService->stop();
    if (wasRunning)
    {
        emit geocodingFinished();
    }
}

bool DocumentManager::isGeocoding() const
{
    return m_geocodingService->isRunning();
}

void DocumentManager::onFamilyGeocoded(const FamilyId& id, double latitude, double longitude)
{
    // A result for a directory this device may no longer write is dropped.
    if (!mayWriteDirectory())
    {
        return;
    }

    // Silent update - no undo/redo, no signal (nothing needs to rebuild for coord changes)
    m_session->edit(DocumentSession::Edit::Directory, [&](Document& document)
    {
        std::optional<Family> existing = document.findFamilyById(id);
        if (existing.has_value())
        {
            Family updated = *existing;
            updated.setLocation(latitude, longitude);
            document.updateFamily(updated);
        }
    });
}

void DocumentManager::onGeocodingFinished()
{
    emit geocodingFinished();
}
