#include "DocumentSession.h"

#include "AccessControl.h"
#include "ResponseMerge.h"

using namespace ServerContract;
using namespace std::placeholders;
using ServerVersion::Fetched;

namespace
{

DocumentSession::Download downloadOutcome(Fetched::Outcome outcome)
{
    switch (outcome)
    {
    case Fetched::Outcome::Accepted:
        return DocumentSession::Download::Done;
    case Fetched::Outcome::Removed:
        return DocumentSession::Download::Removed;
    case Fetched::Outcome::NeedsNewerVersion:
        return DocumentSession::Download::NeedsNewerVersion;
    case Fetched::Outcome::CallFailed:
        return DocumentSession::Download::Unreachable;
    case Fetched::Outcome::ServerFault:
        return DocumentSession::Download::ServerFault;
    }

    Q_ASSERT(false);
    return DocumentSession::Download::ServerFault;
}

/// A downloaded ward, written once it validated; nothing is written otherwise.
void finishDownload(DocumentRecords& records,
                    const QString& documentId,
                    const DocumentPaths& paths,
                    const DocumentSession::Downloaded& done,
                    const Fetched& theirs)
{
    if (theirs.outcome == Fetched::Outcome::Removed)
    {
        ServerVersion::forget(records, documentId, paths);
    }

    if (theirs.outcome != Fetched::Outcome::Accepted)
    {
        done(downloadOutcome(theirs.outcome), std::nullopt);
        return;
    }

    if (!DocumentFile::write(paths.file, theirs.bytes(), nullptr)
        || !ServerVersion::writeBase(records, documentId, paths, theirs.sealed, theirs.version, nullptr))
    {
        done(DocumentSession::Download::WriteFailed, std::nullopt);
        return;
    }

    done(DocumentSession::Download::Done, theirs.document);
}

void ignoreRelease(const Result<Done>&)
{
}

} // namespace

// --- Creating and opening --------------------------------------------------------

DocumentSession::DocumentSession(const Context& context,
                                 std::unique_ptr<SessionTimers> timers,
                                 const QString& documentId,
                                 const DocumentPaths& paths,
                                 const DocumentFile::Sealed& sealed,
                                 const DataKey& dataKey,
                                 const Document& document)
    : m_context(context)
    , m_timers(std::move(timers))
    , m_documentId(documentId)
    , m_paths(paths)
    , m_document(document)
    , m_header(sealed.header)
    , m_dataKey(dataKey)
{
    m_timers->onTimeout = std::bind(&DocumentSession::onTimer, this, _1);
    m_base = ServerVersion::readBase(m_documentId, m_paths, m_context.records, m_context.identity.self());
    settleSyncState();
}

DocumentSession::~DocumentSession()
{
    // A failure here has nobody to hear it; the owner learns it from flush().
    flush(nullptr);

    // Released whether the lease is held or its request is still on the way: the
    // release reaches the server after the request did. An unreleased lease
    // expires anyway.
    if (!m_removed && (m_holdsLease || m_leaseRequested))
    {
        m_context.server.releaseLease(m_documentId, nullptr, &ignoreRelease);
    }
}

std::unique_ptr<DocumentSession> DocumentSession::create(const Context& context,
                                                         std::unique_ptr<SessionTimers> timers,
                                                         const QString& documentId,
                                                         const DocumentPaths& paths,
                                                         QString* errorMessage)
{
    AccessRoster roster;
    roster.addPerson(context.identity.asNewPerson());

    Document document = Document::empty();
    document.setAccessRoster(roster);

    DocumentHeader first;
    first.documentId = documentId;
    first.keyGeneration = 1;
    first.saveCounter = 0;

    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const std::optional<DocumentFile::Sealed> sealed = DocumentFile::seal(document, first, dataKey, context.identity);
    if (!sealed.has_value())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = tr("The new ward could not be encrypted.");
        }
        return nullptr;
    }

    if (!DocumentFile::write(paths.file, DocumentFile::encode(sealed.value()), errorMessage))
    {
        return nullptr;
    }

    return std::unique_ptr<DocumentSession>(
        new DocumentSession(context, std::move(timers), documentId, paths, sealed.value(), dataKey, document));
}

std::unique_ptr<DocumentSession> DocumentSession::open(const Context& context,
                                                       std::unique_ptr<SessionTimers> timers,
                                                       const QString& documentId,
                                                       const DocumentPaths& paths,
                                                       LoadFailure& failure,
                                                       QString* refusal)
{
    const std::optional<QByteArray> bytes = DocumentFile::readAll(paths.file);
    if (!bytes.has_value())
    {
        failure = LoadFailure::NotFound;
        return nullptr;
    }

    DocumentFile::Sealed sealed;
    switch (DocumentFile::decode(bytes.value(), sealed))
    {
    case DocumentFile::Read::Ok:
        break;
    case DocumentFile::Read::NeedsNewerVersion:
        failure = LoadFailure::NeedsNewerVersion;
        return nullptr;
    case DocumentFile::Read::Malformed:
        failure = LoadFailure::Refused;
        if (refusal != nullptr)
        {
            *refusal = tr("The ward's file is not readable.");
        }
        return nullptr;
    }

    // Opening a local file is a download with no base copy: one statement of the
    // rules serves the local file and every download alike.
    const Reconcile::DownloadCheck check =
        Reconcile::validateDownload(documentId,
                                    std::nullopt,
                                    {sealed.header},
                                    sealed.ciphertext,
                                    context.identity.self(),
                                    context.records.record(documentId).highWaterMark);
    if (check.verdict != Reconcile::Verdict::Accept)
    {
        failure = LoadFailure::Refused;
        if (refusal != nullptr)
        {
            *refusal = check.reason;
        }
        return nullptr;
    }

    std::unique_ptr<DocumentSession> session(new DocumentSession(context,
                                                                 std::move(timers),
                                                                 documentId,
                                                                 paths,
                                                                 sealed,
                                                                 check.dataKey.value(),
                                                                 check.document.value()));
    failure = LoadFailure::None;

    if (session->isShared())
    {
        session->startShared();
    }
    return session;
}

void DocumentSession::download(const Context& context,
                               const QString& documentId,
                               const DocumentPaths& paths,
                               QObject* receiver,
                               Downloaded done)
{
    ServerVersion::fetch(context.identity,
                         context.records,
                         context.server,
                         documentId,
                         paths,
                         receiver,
                         std::bind(&finishDownload, std::ref(context.records), documentId, paths, done, _1));
}

bool DocumentSession::isShared() const
{
    return m_context.records.record(m_documentId).baseVersion.has_value();
}

// --- Editing and saving ----------------------------------------------------------

DocumentSession::Refusal DocumentSession::edit(Edit kind, const std::function<void(Document&)>& change)
{
    if (kind == Edit::Directory)
    {
        if (isDirectoryReadOnly())
        {
            return Refusal::DirectoryReadOnly;
        }
        if (m_requiresUpdate)
        {
            return Refusal::RequiresUpdate;
        }
    }

    change(m_document);
    m_saveScheduled = true;
    m_timers->start(SessionTimers::Timer::Save, SaveDelayMilliseconds);
    return Refusal::None;
}

bool DocumentSession::flush(QString* errorMessage)
{
    if (!m_saveScheduled)
    {
        return true;
    }
    return writeSave(errorMessage);
}

void DocumentSession::publishNow()
{
    QString error;
    if (!flush(&error))
    {
        emit saveFailed(error);
    }

    m_timers->stop(SessionTimers::Timer::Retry);
    publish();
}

void DocumentSession::checkServer()
{
    checkHead();
}

bool DocumentSession::isDirectoryReadOnly() const
{
    return isShared() && m_leaseHeldElsewhere;
}

bool DocumentSession::mayWriteDirectory() const
{
    return !isShared() || m_holdsLease;
}

void DocumentSession::setEmergencyActive(bool active)
{
    // Unchanged, the head check keeps its schedule.
    if (active == m_emergencyActive)
    {
        return;
    }
    m_emergencyActive = active;
    if (isShared() && !m_removed)
    {
        restartHeadCheck();
    }
}

void DocumentSession::onTimer(SessionTimers::Timer timer)
{
    switch (timer)
    {
    case SessionTimers::Timer::Save:
    {
        QString error;
        if (!writeSave(&error))
        {
            emit saveFailed(error);
            return;
        }
        publish();
        break;
    }
    case SessionTimers::Timer::Retry:
        publish();
        break;
    case SessionTimers::Timer::Heartbeat:
        takeLease();
        break;
    case SessionTimers::Timer::HeadCheck:
        checkHead();
        restartHeadCheck();
        break;
    }
}

bool DocumentSession::writeSave(QString* errorMessage)
{
    m_timers->stop(SessionTimers::Timer::Save);

    // A removed ward's files are gone, and its document goes with the session.
    if (m_removed)
    {
        m_saveScheduled = false;
        return true;
    }

    if (!writeLocal(m_document, m_header, m_dataKey))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = tr("The ward could not be saved to %1.").arg(m_paths.file);
        }
        return false;
    }

    m_saveScheduled = false;
    settleSyncState();
    return true;
}

bool DocumentSession::writeLocal(const Document& document, const DocumentHeader& previous, const DataKey& dataKey)
{
    const std::optional<DocumentFile::Sealed> sealed =
        DocumentFile::seal(document, previous, dataKey, m_context.identity);
    if (!sealed.has_value() || !DocumentFile::write(m_paths.file, DocumentFile::encode(sealed.value()), nullptr))
    {
        return false;
    }

    m_header = sealed.value().header;
    m_dataKey = dataKey;
    install(document);
    return true;
}

bool DocumentSession::writeLocal(const Fetched& theirs)
{
    if (!DocumentFile::write(m_paths.file, theirs.bytes(), nullptr))
    {
        return false;
    }

    m_header = theirs.sealed.header;
    m_dataKey = theirs.dataKey;
    install(theirs.document);
    return true;
}

void DocumentSession::install(const Document& document)
{
    // An edit's own save writes the document already in memory.
    if (&document == &m_document)
    {
        return;
    }

    const bool replaced = document != m_document;
    m_document = document;
    if (replaced)
    {
        emit documentReplaced();
    }
}

bool DocumentSession::hasUnpublishedChanges() const
{
    // The record's high-water mark is the base version's header, so this needs no
    // base copy.
    return m_saveScheduled
        || m_header.saveCounter > m_context.records.record(m_documentId).highWaterMark.saveCounter;
}

// --- The server ------------------------------------------------------------------

void DocumentSession::startShared()
{
    takeLease();
    checkHead();
    restartHeadCheck();
}

void DocumentSession::checkHead()
{
    // Paused while a conflict waits for resolve.
    if (!isShared() || m_removed || m_conflict.has_value())
    {
        return;
    }

    // One asked for while another operation is in flight runs when it ends, since
    // what prompted it may be newer than that operation.
    if (m_busy)
    {
        m_checkAgain = true;
        return;
    }

    m_busy = true;
    m_context.server.head(m_documentId, this, std::bind(&DocumentSession::onHead, this, _1));
}

void DocumentSession::onHead(const Result<Head>& head)
{
    m_busy = false;

    if (head.error == ServerError::NotAllowListed)
    {
        proveRemoval();
        return;
    }

    if (!head.ok())
    {
        // A call that did not get through. There is no lease to take, so the
        // session is editable; reconciliation happens on reconnect.
        setLease(false, false);
        setSyncState(SyncState::Unreachable);
        finishOperation();
        return;
    }

    const std::optional<Fingerprint>& holder = head.value->leaseHolder;
    if (holder.has_value() && holder.value() != m_context.identity.fingerprint())
    {
        setLease(false, true);
    }
    else if (!m_holdsLease)
    {
        takeLease();
    }

    if (head.value->version > m_context.records.record(m_documentId).baseVersion.value_or(0))
    {
        fetchTheirs();
        return;
    }

    finishOperation();
    publish();
}

void DocumentSession::fetchTheirs()
{
    // Pending edits are written first, so what is unpublished is on disk and in
    // the decision below.
    QString error;
    if (!flush(&error))
    {
        emit saveFailed(error);
    }

    m_busy = true;
    ServerVersion::fetch(m_context.identity,
                         m_context.records,
                         m_context.server,
                         m_documentId,
                         m_paths,
                         this,
                         std::bind(&DocumentSession::onFetched, this, _1));
}

void DocumentSession::onFetched(const Fetched& theirs)
{
    m_busy = false;

    switch (theirs.outcome)
    {
    case Fetched::Outcome::Accepted:
        setRequiresUpdate(false);
        adopt(theirs);
        break;
    case Fetched::Outcome::Removed:
        finishRemoved(theirs.remover, theirs.left);
        return;
    case Fetched::Outcome::NeedsNewerVersion:
        // The local file and record stay untouched; re-evaluated on every head
        // check, and cleared once the server serves a version this build reads.
        setRequiresUpdate(true);
        settleSyncState();
        break;
    case Fetched::Outcome::CallFailed:
        setSyncState(SyncState::Unreachable);
        break;
    case Fetched::Outcome::ServerFault:
        setFault(theirs.reason);
        break;
    }

    finishOperation();
}

void DocumentSession::publish()
{
    // Nothing publishes while the server serves a version this build cannot read,
    // or while a conflict waits for resolve.
    if (!isShared() || m_removed || m_requiresUpdate || m_conflict.has_value())
    {
        return;
    }

    if (m_busy)
    {
        m_publishAgain = true;
        return;
    }

    QString error;
    if (!flush(&error))
    {
        emit saveFailed(error);
        return;
    }

    if (!hasUnpublishedChanges())
    {
        settleSyncState();
        return;
    }

    const std::optional<QByteArray> bytes = DocumentFile::readAll(m_paths.file);
    DocumentFile::Sealed sealed;
    if (!bytes.has_value() || DocumentFile::decode(bytes.value(), sealed) != DocumentFile::Read::Ok)
    {
        emit saveFailed(tr("The ward could not be read back from %1 to publish it.").arg(m_paths.file));
        return;
    }

    // One pending upload, never a list: the local file is the latest save, so it
    // supersedes every earlier one.
    m_busy = true;
    m_publishAgain = false;
    setSyncState(SyncState::PendingUpload);
    m_context.server.upload(m_documentId,
                            m_context.records.record(m_documentId).baseVersion.value(),
                            Upload{sealed.header, sealed.ciphertext},
                            this,
                            std::bind(&DocumentSession::onPublished, this, _1, sealed));
}

void DocumentSession::onPublished(const Result<qint64>& result, const DocumentFile::Sealed& published)
{
    m_busy = false;

    switch (result.error)
    {
    case ServerError::None:
    {
        QString error;
        if (!ServerVersion::writeBase(m_context.records, m_documentId, m_paths, published, result.value.value(), &error))
        {
            emit saveFailed(error);
        }
        m_base = ServerVersion::readBase(m_documentId, m_paths, m_context.records, m_context.identity.self());

        m_retryDelay = SessionTimers::FirstRetryMilliseconds;
        m_timers->stop(SessionTimers::Timer::Retry);
        settleSyncState();

        // A save made while this publish was in flight goes next.
        if (hasUnpublishedChanges())
        {
            m_publishAgain = true;
        }
        finishOperation();
        return;
    }

    case ServerError::VersionConflict:
        // The server refuses only when its version moved past the base, so theirs
        // is newer; a revocation is itself a new version, so a moved key
        // generation always arrives this way too.
        fetchTheirs();
        return;

    case ServerError::NotAllowListed:
        proveRemoval();
        return;

    case ServerError::Unreachable:
    case ServerError::RateLimited:
        // A state, never an error.
        setSyncState(SyncState::Unreachable);
        scheduleRetry();
        finishOperation();
        return;

    default:
        setFault(tr("The server refused this ward's latest version."));
        finishOperation();
        return;
    }
}

void DocumentSession::scheduleRetry()
{
    // Starting at 5 seconds and doubling to at most 5 minutes: a ward on a phone
    // tether during an emergency keeps its connection free between attempts.
    m_timers->start(SessionTimers::Timer::Retry, m_retryDelay);
    m_retryDelay = qMin(m_retryDelay * 2, SessionTimers::LastRetryMilliseconds);
}

void DocumentSession::proveRemoval()
{
    if (m_removed)
    {
        return;
    }

    m_busy = true;
    ServerVersion::proveRemoval(m_context.identity,
                                m_context.records,
                                m_context.server,
                                m_documentId,
                                m_paths,
                                this,
                                std::bind(&DocumentSession::onRemovalChecked, this, _1));
}

void DocumentSession::onRemovalChecked(const Fetched& proof)
{
    m_busy = false;

    switch (proof.outcome)
    {
    case Fetched::Outcome::Removed:
        finishRemoved(proof.remover, proof.left);
        return;
    case Fetched::Outcome::CallFailed:
        setSyncState(SyncState::Unreachable);
        break;
    case Fetched::Outcome::Accepted:
    case Fetched::Outcome::NeedsNewerVersion:
    case Fetched::Outcome::ServerFault:
        // Only a chain-proven removal is acted on; anything else deletes nothing.
        setFault(proof.reason);
        break;
    }

    finishOperation();
}

void DocumentSession::finishOperation()
{
    if (m_checkAgain)
    {
        m_checkAgain = false;
        checkHead();
    }

    if (m_publishAgain)
    {
        publish();
    }
}

// --- Taking in their version -----------------------------------------------------

void DocumentSession::adopt(const Fetched& theirs)
{
    if (!hasUnpublishedChanges())
    {
        fastForward(theirs);
        return;
    }

    // The refused-publish route. Their data key, generation, and roster are taken
    // exactly as validated; every roster change of this device went through the
    // queue, so nothing of this session's roster needs carrying over.
    if (!m_base.has_value())
    {
        raiseConflict(theirs);
        return;
    }

    const std::optional<Document> merged =
        ResponseMerge::mergeDocuments(m_base.value().document, m_document, theirs.document);
    if (!merged.has_value())
    {
        raiseConflict(theirs);
        return;
    }

    // Including this device's own publish whose answer was lost: it merges to
    // theirs, and nothing publishes a second time.
    if (merged.value() == theirs.document)
    {
        fastForward(theirs);
        return;
    }

    mergeWith(theirs, merged.value());
}

void DocumentSession::fastForward(const Fetched& theirs)
{
    const DocumentHeader previousBase = m_base.has_value() ? m_base.value().sealed.header : m_header;
    const AccessRoster previousRoster = baseRoster();

    if (!writeLocal(theirs))
    {
        emit saveFailed(tr("The ward could not be saved to %1.").arg(m_paths.file));
        return;
    }
    QString baseError;
    if (!moveBase(theirs, &baseError))
    {
        emit saveFailed(baseError);
        return;
    }

    settleSyncState();
    emit versionAdopted(previousBase, theirs.laterHeaders, previousRoster);
}

void DocumentSession::mergeWith(const Fetched& theirs, const Document& merged)
{
    const DocumentHeader previousBase = m_base.value().sealed.header;
    const AccessRoster previousRoster = baseRoster();

    // The local file first, then the base, so unpublished work is never covered by
    // a base that moved over a file that did not.
    if (!writeLocal(merged, theirs.sealed.header, theirs.dataKey))
    {
        emit saveFailed(tr("The ward could not be saved to %1.").arg(m_paths.file));
        return;
    }
    QString baseError;
    if (!moveBase(theirs, &baseError))
    {
        emit saveFailed(baseError);
        return;
    }

    emit versionAdopted(previousBase, theirs.laterHeaders, previousRoster);
    publish();
}

void DocumentSession::raiseConflict(const Fetched& theirs)
{
    // The local file takes this session's work under their roster and key, so
    // every later save builds on it. The base stays where it was, so a crash
    // before resolve meets the same conflict on reopen.
    Document mine = m_document;
    mine.setAccessRoster(theirs.document.accessRoster());
    if (!writeLocal(mine, theirs.sealed.header, theirs.dataKey))
    {
        emit saveFailed(tr("The ward could not be saved to %1.").arg(m_paths.file));
        return;
    }

    m_conflict = theirs;
    m_timers->stop(SessionTimers::Timer::Retry);
    settleSyncState();
    emit versionAdopted(m_base.has_value() ? m_base.value().sealed.header : m_header,
                        theirs.laterHeaders,
                        baseRoster());
    emit conflictRaised(Conflict{nameOf(theirs.sealed.header.lastWriter), theirs.sealed.header.lastWriteTime});
}

std::optional<DocumentSession::Conflict> DocumentSession::pendingConflict() const
{
    if (!m_conflict.has_value())
    {
        return std::nullopt;
    }
    const DocumentHeader& theirs = m_conflict.value().sealed.header;
    return Conflict{nameOf(theirs.lastWriter), theirs.lastWriteTime};
}

bool DocumentSession::resolve(Resolution resolution, QString* errorMessage)
{
    if (!m_conflict.has_value())
    {
        return true;
    }

    const Fetched theirs = m_conflict.value();
    m_conflict.reset();

    std::optional<EmergencyResponse> baseResponse;
    if (m_base.has_value())
    {
        baseResponse = m_base.value().document.emergencyResponse();
    }
    const std::optional<EmergencyResponse> response =
        ResponseMerge::merge(baseResponse, m_document.emergencyResponse(), theirs.document.emergencyResponse());

    // Keep theirs: their directory with the merged response. Keep mine: the local
    // file as it stands - this session's directory, the merged response, and the
    // adopted roster and key - which was sealed on their header, so it passes every
    // server rule and keeps everyone they removed off it.
    Document resolved = resolution == Resolution::KeepTheirs ? theirs.document : m_document;
    resolved.setEmergencyResponse(response);

    bool written = false;
    if (resolved == theirs.document)
    {
        written = writeLocal(theirs);
    }
    else
    {
        const DocumentHeader& previous = resolution == Resolution::KeepMine ? m_header : theirs.sealed.header;
        written = writeLocal(resolved, previous, theirs.dataKey);
    }

    // The local file first, as in a merge; the base then moves to theirs. A base
    // not written leaves the resolved file ahead of the old base, so the next head
    // check takes theirs in again and nothing is lost.
    if (!written)
    {
        m_conflict = theirs;
        if (errorMessage != nullptr)
        {
            *errorMessage = tr("The ward could not be saved to %1.").arg(m_paths.file);
        }
        return false;
    }
    if (!moveBase(theirs, errorMessage))
    {
        settleSyncState();
        restartHeadCheck();
        return false;
    }

    settleSyncState();
    publish();
    return true;
}

bool DocumentSession::moveBase(const Fetched& theirs, QString* errorMessage)
{
    if (!ServerVersion::writeBase(m_context.records, m_documentId, m_paths, theirs.sealed, theirs.version, errorMessage))
    {
        return false;
    }

    m_base = ServerVersion::BaseCopy{theirs.sealed, theirs.document, theirs.dataKey};
    return true;
}

void DocumentSession::finishRemoved(const Fingerprint& remover, bool left)
{
    if (m_removed)
    {
        return;
    }

    const QString byWhom = nameOf(remover);

    stopEveryTimer();
    m_removed = true;
    m_saveScheduled = false;
    ServerVersion::forget(m_context.records, m_documentId, m_paths);
    emit removed(byWhom, left);
}

AccessRoster DocumentSession::baseRoster() const
{
    return m_base.has_value() ? m_base.value().document.accessRoster() : m_document.accessRoster();
}

QString DocumentSession::nameOf(const Fingerprint& device) const
{
    std::optional<RosterPerson> person;
    if (m_conflict.has_value())
    {
        person = m_conflict.value().document.accessRoster().personFor(device);
    }
    if (!person.has_value())
    {
        person = m_document.accessRoster().personFor(device);
    }
    if (!person.has_value() && m_base.has_value())
    {
        person = m_base.value().document.accessRoster().personFor(device);
    }

    return person.has_value() ? person.value().displayName() : tr("a former member");
}

// --- Sharing ---------------------------------------------------------------------

void DocumentSession::share(const QList<RosterDevice>& linkedDevices)
{
    if (isShared() || m_busy || m_removed)
    {
        return;
    }

    // Devices linked while the ward was unshared join the first header: until now
    // only this device read the file, so this is the one moment it matters.
    AccessRoster roster = m_document.accessRoster();
    for (const RosterDevice& device : linkedDevices)
    {
        const std::optional<AccessChange> linked =
            AccessControl::linkDevice(m_header, roster, m_dataKey, m_context.identity.fingerprint(), device);
        if (linked.has_value())
        {
            roster = linked.value().roster;
        }
    }

    // Through the ordinary write: the new roster, and any edit still scheduled.
    if (roster != m_document.accessRoster())
    {
        Document document = m_document;
        document.setAccessRoster(roster);
        if (!writeLocal(document, m_header, m_dataKey))
        {
            emit saveFailed(tr("The ward could not be saved to %1.").arg(m_paths.file));
            return;
        }
        m_saveScheduled = false;
        m_timers->stop(SessionTimers::Timer::Save);
    }
    else
    {
        QString error;
        if (!flush(&error))
        {
            emit saveFailed(error);
            return;
        }
    }

    // Registered under its existing id; its header, listing this person's
    // devices, becomes the first allow-list.
    DocumentFile::Sealed sealed;
    if (DocumentFile::decode(DocumentFile::readAll(m_paths.file).value_or(QByteArray()), sealed) != DocumentFile::Read::Ok)
    {
        emit saveFailed(tr("The ward could not be read back from %1 to share it.").arg(m_paths.file));
        return;
    }

    m_busy = true;
    m_context.server.registerDocument(Upload{sealed.header, sealed.ciphertext},
                                      this,
                                      std::bind(&DocumentSession::onRegistered, this, _1, sealed));
}

void DocumentSession::onRegistered(const Result<qint64>& result, const DocumentFile::Sealed& registered)
{
    m_busy = false;

    if (!result.ok())
    {
        emit shareFailed(result.error);
        return;
    }

    // The returned version becomes the base, which is what makes the ward shared.
    QString error;
    if (!ServerVersion::writeBase(m_context.records, m_documentId, m_paths, registered, result.value.value(), &error))
    {
        emit saveFailed(error);
        return;
    }

    m_base = ServerVersion::readBase(m_documentId, m_paths, m_context.records, m_context.identity.self());
    settleSyncState();
    emit readOnlyChanged();
    emit shared();
    startShared();
}

// --- The lease -------------------------------------------------------------------

void DocumentSession::takeLease()
{
    if (m_leaseRequested || m_removed)
    {
        return;
    }

    m_leaseRequested = true;
    m_context.server.takeLease(m_documentId, this, std::bind(&DocumentSession::onLease, this, _1));
}

void DocumentSession::onLease(const Result<Lease>& lease)
{
    m_leaseRequested = false;

    switch (lease.error)
    {
    case ServerError::None:
        setLease(true, false);
        m_timers->start(SessionTimers::Timer::Heartbeat, HeartbeatMilliseconds);
        return;

    case ServerError::LeaseHeld:
        // Tried again on each head check; a lease whose heartbeat stopped is taken
        // over once the server lets it expire.
        setLease(false, true);
        m_timers->stop(SessionTimers::Timer::Heartbeat);
        return;

    case ServerError::NotAllowListed:
        if (!m_busy)
        {
            proveRemoval();
        }
        return;

    default:
        // The lease cannot speak for a disconnected device: with no lease to take,
        // the session is editable, and the heartbeat keeps asking.
        setLease(false, false);
        m_timers->start(SessionTimers::Timer::Heartbeat, HeartbeatMilliseconds);
        return;
    }
}

void DocumentSession::setLease(bool held, bool heldElsewhere)
{
    const bool readOnly = isDirectoryReadOnly();
    const bool mayWrite = mayWriteDirectory();

    m_holdsLease = held;
    m_leaseHeldElsewhere = heldElsewhere;

    if (readOnly != isDirectoryReadOnly() || mayWrite != mayWriteDirectory())
    {
        emit readOnlyChanged();
    }
}

// --- State -----------------------------------------------------------------------

void DocumentSession::setSyncState(SyncState state)
{
    if (state != SyncState::ServerFault)
    {
        m_faultReason.clear();
    }

    if (m_syncState != state)
    {
        m_syncState = state;
        emit syncStateChanged(state);
    }
}

void DocumentSession::setFault(const QString& reason)
{
    // A new reason in the same state is news to the indicator too.
    const bool reasonChanged = m_syncState == SyncState::ServerFault && m_faultReason != reason;
    m_faultReason = reason;
    setSyncState(SyncState::ServerFault);
    if (reasonChanged)
    {
        emit syncStateChanged(m_syncState);
    }
}

void DocumentSession::settleSyncState()
{
    if (!isShared())
    {
        setSyncState(SyncState::Local);
        return;
    }

    setSyncState(hasUnpublishedChanges() ? SyncState::PendingUpload : SyncState::UpToDate);
}

void DocumentSession::setRequiresUpdate(bool requiresUpdate)
{
    if (m_requiresUpdate != requiresUpdate)
    {
        m_requiresUpdate = requiresUpdate;
        emit requiresUpdateChanged();
    }
}

void DocumentSession::restartHeadCheck()
{
    // Every 30 seconds during an emergency, so connected responders see each
    // other's records within about half a minute.
    m_timers->start(SessionTimers::Timer::HeadCheck,
                    m_emergencyActive ? EmergencyHeadCheckMilliseconds : HeadCheckMilliseconds);
}

void DocumentSession::stopEveryTimer()
{
    m_timers->stop(SessionTimers::Timer::Save);
    m_timers->stop(SessionTimers::Timer::Retry);
    m_timers->stop(SessionTimers::Timer::Heartbeat);
    m_timers->stop(SessionTimers::Timer::HeadCheck);
}
