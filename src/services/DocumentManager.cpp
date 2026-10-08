#include "DocumentManager.h"
#include "BackgroundGeocodingService.h"
#include "RosterChangeQueue.h"
#include "ServerConnection.h"
#include "UnitLookupService.h"

#include <QDebug>

using namespace std::placeholders;

DocumentManager* DocumentManager::s_instance = nullptr;

DocumentManager::DocumentManager(const DeviceIdentity& identity,
                                 DocumentRecords& records,
                                 ServerConnection& server,
                                 DocumentLibrary& library,
                                 RosterChangeQueue& queue,
                                 QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_records(&records)
    , m_server(&server)
    , m_library(&library)
    , m_queue(&queue)
    , m_empty(Document::empty())
    , m_commandHistory(this)
{
    connectServices();

    connect(m_queue, &RosterChangeQueue::removed, this, &DocumentManager::onQueueRemoved);
}

DocumentManager::DocumentManager(const DeviceIdentity& identity,
                                 std::unique_ptr<DocumentSession> session,
                                 QObject* parent)
    : QObject(parent)
    , m_identity(identity)
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
    if (m_queue != nullptr)
    {
        m_queue->setOpenSession(nullptr);
    }
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
    m_fetching.clear();

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
    m_fetching.clear();
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
    m_library->fetchWard(documentId, *m_server, this, std::bind(&DocumentManager::onFetched, this, documentId, _1));
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
    case DocumentSession::Download::Removed:
        forgetRemovedWard(documentId, QString(), false);
        return;
    case DocumentSession::Download::Unreachable:
    case DocumentSession::Download::NeedsNewerVersion:
    case DocumentSession::Download::ServerFault:
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
    m_fetching.clear();

    m_copy = copy;
    onDocumentReplaced();
    emit documentOpened();
    emit readOnlyChanged();
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
    if (m_queue != nullptr)
    {
        m_queue->setOpenSession(nullptr);
    }
    m_session.reset();
    return true;
}

void DocumentManager::resetWard()
{
    m_copy.reset();
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
    m_pendingWardLookups.clear();
    m_pendingStakeLookups.clear();

    DocumentSession* opened = m_session.get();
    connect(opened, &DocumentSession::documentReplaced, this, &DocumentManager::onDocumentReplaced);
    connect(opened, &DocumentSession::readOnlyChanged, this, &DocumentManager::onReadOnlyChanged);
    connect(opened, &DocumentSession::syncStateChanged, this, &DocumentManager::syncStateChanged);
    connect(opened, &DocumentSession::conflictRaised, this, &DocumentManager::conflictRaised);
    connect(opened, &DocumentSession::saveFailed, this, &DocumentManager::saveFailed);
    connect(opened, &DocumentSession::removed, this, &DocumentManager::onSessionRemoved);
    if (m_queue != nullptr)
    {
        m_queue->setOpenSession(opened);
    }

    opened->setEmergencyActive(opened->document().emergencyResponse().has_value());
    onDocumentReplaced();
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

void DocumentManager::onSessionRemoved(const QString& byWhom, bool left)
{
    const QString documentId = m_session->documentId();

    // Finished from inside its own signal, so it goes once that returns.
    stopGeocoding();
    if (m_queue != nullptr)
    {
        m_queue->setOpenSession(nullptr);
    }
    m_session.release()->deleteLater();
    resetWard();
    forgetRemovedWard(documentId, byWhom, left);
}

void DocumentManager::onQueueRemoved(const QString& documentId, bool left)
{
    // The open ward finishes through its session, which has already told the
    // person; a ward no longer in the library has gone that way.
    if ((m_session && m_session->documentId() == documentId) || !m_library->contains(documentId))
    {
        return;
    }
    forgetRemovedWard(documentId, QString(), left);
}

void DocumentManager::forgetRemovedWard(const QString& documentId, const QString& byWhom, bool left)
{
    const QString wardName = wardNameOf(documentId);
    if (m_library != nullptr)
    {
        m_library->forgetRemoved(documentId);
    }
    emit accessRemoved(wardName, byWhom, left);
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
    return m_session && !m_copy.has_value() && m_session->mayWriteDirectory();
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
