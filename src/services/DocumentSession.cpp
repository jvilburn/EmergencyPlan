#include "DocumentSession.h"

#include "ResponseMerge.h"

#include <QPointer>

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
    case Fetched::Outcome::AccessLost:
        return DocumentSession::Download::AccessLost;
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
    if (theirs.outcome == Fetched::Outcome::AccessLost)
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

/// An invite's keypairs, both from its seed through their own hash domains.
struct InviteKeys
{
    Keypair sealing;
    SigningKeypair signing;

    DeviceKeys keys() const { return DeviceKeys{sealing.publicKey, signing.verifyKey}; }
    Fingerprint fingerprint() const { return DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey); }
    Reconcile::Self self() const { return Reconcile::Self{sealing, signing.verifyKey}; }
};

InviteKeys inviteKeys(const QByteArray& seed)
{
    InviteKeys keys;
    keys.sealing = DocumentCrypto::keypairFromSeed(
        DocumentCrypto::hash(HashDomain::InviteSealing, seed, DocumentCrypto::SigningSeedSize));
    keys.signing = DocumentCrypto::signingKeypairFromSeed(
        DocumentCrypto::hash(HashDomain::InviteSigning, seed, DocumentCrypto::SigningSeedSize));
    return keys;
}

/// The joined version landed, or not: the local file and base are written only once
/// the server holds the version they are of.
void onJoinUploaded(DocumentRecords& records,
                    const QString& documentId,
                    const DocumentPaths& paths,
                    const DocumentFile::Sealed& sealed,
                    const DocumentSession::Joined& done,
                    const Result<qint64>& result)
{
    switch (result.error)
    {
    case ServerError::None:
        break;
    case ServerError::NotAKeyHolder:
        done(DocumentSession::Join::InviteUsed, QString());
        return;
    case ServerError::UnknownDocument:
        done(DocumentSession::Join::UnknownWard, QString());
        return;
    case ServerError::Unreachable:
    case ServerError::RateLimited:
        done(DocumentSession::Join::Unreachable, QString());
        return;
    default:
        done(DocumentSession::Join::ServerFault, QObject::tr("The server refused the version that joins this device."));
        return;
    }

    if (!DocumentFile::write(paths.file, DocumentFile::encode(sealed), nullptr)
        || !ServerVersion::writeBase(records, documentId, paths, sealed, result.value.value(), nullptr))
    {
        done(DocumentSession::Join::WriteFailed, QString());
        return;
    }
    done(DocumentSession::Join::Done, QString());
}

/// The current version, read as the invite: this device takes the invite's place and
/// the result goes up signed by the invite. A version that moved meanwhile is read
/// again, once.
void onJoinFetched(const DocumentSession::Context& context,
                   const FileOpenRouter::InviteLink& link,
                   const InviteKeys& keys,
                   const DocumentPaths& paths,
                   QObject* receiver,
                   const DocumentSession::Joined& done,
                   bool mayRetry,
                   const Fetched& theirs)
{
    switch (theirs.outcome)
    {
    case Fetched::Outcome::Accepted:
        break;
    case Fetched::Outcome::AccessLost:
        done(DocumentSession::Join::InviteUsed, QString());
        return;
    case Fetched::Outcome::NeedsNewerVersion:
        done(DocumentSession::Join::NeedsNewerVersion, QString());
        return;
    case Fetched::Outcome::CallFailed:
        done(theirs.error == ServerError::UnknownDocument ? DocumentSession::Join::UnknownWard
                                                          : DocumentSession::Join::Unreachable,
             QString());
        return;
    case Fetched::Outcome::ServerFault:
        done(DocumentSession::Join::ServerFault, theirs.reason);
        return;
    }

    Document document = theirs.document;
    SealingSet roster = document.sealingSet();
    if (!roster.join(keys.fingerprint(), context.identity.device(), context.identity.member, context.identity.displayName))
    {
        done(DocumentSession::Join::ServerFault, QObject::tr("The version does not carry this invite."));
        return;
    }
    document.setSealingSet(roster);

    const ServerVersion::Resealed resealed = ServerVersion::reseal(document, theirs.dataKey, theirs.sealed.header);
    const std::optional<DocumentFile::Sealed> sealed =
        DocumentFile::seal(resealed.document, theirs.sealed.header, resealed.dataKey);
    if (!sealed.has_value())
    {
        done(DocumentSession::Join::ServerFault, QObject::tr("The version could not be sealed on this device."));
        return;
    }

    const QPointer<QObject> alive(receiver);
    context.server.uploadAs(
        ServerConnection::Writer{keys.keys(), keys.signing.signingKey},
        link.documentId,
        theirs.version,
        Upload{sealed->header, sealed->ciphertext},
        receiver,
        [context, link, keys, paths, receiver, done, mayRetry, sealed, alive](const Result<qint64>& result) {
            if (result.error == ServerError::VersionConflict && mayRetry && alive)
            {
                ServerVersion::fetch(keys.self(), {}, context.server, link.documentId, receiver,
                                     std::bind(&onJoinFetched, context, link, keys, paths, receiver, done, false, _1));
                return;
            }
            onJoinUploaded(context.records, link.documentId, paths, sealed.value(), done, result);
        });
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
    if (!m_lost && (m_holdsLease || m_leaseRequested))
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
    Document document = Document::empty();
    document.setSealingSet(SealingSet::of(context.identity.member, context.identity.displayName, context.identity.device()));

    DocumentHeader first;
    first.documentId = documentId;

    const DataKey dataKey = DocumentCrypto::generateDataKey();
    const std::optional<DocumentFile::Sealed> sealed = DocumentFile::seal(document, first, dataKey);
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

    // One statement of the rules serves the local file and every download alike.
    const Reconcile::DownloadCheck check =
        Reconcile::validate(documentId, sealed.header, sealed.ciphertext, context.identity.self(), {});
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
    ServerVersion::fetch(context.identity.self(),
                         ServerVersion::trustedKeys(documentId, paths, context.records, context.identity.self()),
                         context.server,
                         documentId,
                         receiver,
                         std::bind(&finishDownload, std::ref(context.records), documentId, paths, done, _1));
}

void DocumentSession::join(const Context& context,
                           const FileOpenRouter::InviteLink& link,
                           const DocumentPaths& paths,
                           QObject* receiver,
                           Joined done)
{
    const InviteKeys keys = inviteKeys(link.seed);
    ServerVersion::fetch(keys.self(),
                         {},
                         context.server,
                         link.documentId,
                         receiver,
                         std::bind(&onJoinFetched, context, link, keys, paths, receiver, done, true, _1));
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
    if (isShared() && !m_lost)
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

    // A lost ward's files are gone, and its document goes with the session.
    if (m_lost)
    {
        m_saveScheduled = false;
        return true;
    }

    // The sealing-set rule against the version this save builds on. The new set and
    // key reach memory only once written, so a failed save leaves the old pair, and
    // the next save rotates afresh.
    const ServerVersion::Resealed resealed = ServerVersion::reseal(m_document, m_dataKey, m_header);
    if (!writeLocal(resealed.document, m_header, resealed.dataKey))
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

QString DocumentSession::nameOf(const Document& document, const Fingerprint& device) const
{
    const std::optional<Holder> person = document.sealingSet().personOf(device);
    return person.has_value() ? person->name : tr("another member");
}

bool DocumentSession::writeLocal(const Document& document, const DocumentHeader& previous, const DataKey& dataKey)
{
    const std::optional<DocumentFile::Sealed> sealed = DocumentFile::seal(document, previous, dataKey);
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

    // Who the document is sealed to is not content, so a reseal keeps undo.
    Document content = document;
    content.setSealingSet(m_document.sealingSet());
    const bool replaced = content != m_document;
    m_document = document;
    if (replaced)
    {
        emit documentReplaced();
    }
}

bool DocumentSession::hasUnpublishedChanges() const
{
    // The record holds the base copy's save counter, so this needs no base copy.
    return m_saveScheduled || m_header.saveCounter > m_context.records.record(m_documentId).baseSaveCounter;
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
    if (!isShared() || m_lost || m_conflict.has_value())
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

    if (head.error == ServerError::UnknownDocument)
    {
        setFault(tr("The server no longer has this ward."));
        finishOperation();
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

    // A current version not sealed to this device is one this device no longer holds;
    // the roster it last read says who wrote it out. A header this build cannot read
    // is judged by the fetch below.
    DocumentHeader header;
    if (DocumentHeader::parse(head.value->header, header) == HeaderParse::Ok
        && !header.entryFor(m_context.identity.fingerprint()).has_value())
    {
        finishLost(ServerVersion::whyLost(m_document.sealingSet(),
                                          m_context.identity.fingerprint(),
                                          head.value->published.device));
        return;
    }

    m_head = head.value.value();
    emit headChanged();

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

    QList<DataKey> trusted{m_dataKey};
    if (m_base.has_value())
    {
        trusted.append(m_base.value().dataKey);
    }

    m_busy = true;
    ServerVersion::fetch(m_context.identity.self(),
                         trusted,
                         m_context.server,
                         m_documentId,
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
    case Fetched::Outcome::AccessLost:
        finishLost(ServerVersion::whyLost(m_document.sealingSet(),
                                          m_context.identity.fingerprint(),
                                          theirs.published.device));
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
    if (!isShared() || m_lost || m_requiresUpdate || m_conflict.has_value())
    {
        return;
    }

    if (m_busy)
    {
        m_publishAgain = true;
        return;
    }
    m_publishAgain = false;

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
        // is newer.
        fetchTheirs();
        return;

    case ServerError::NotAKeyHolder:
        // The current version does not seal to this device: the head says who wrote
        // it out.
        m_publishAgain = false;
        checkHead();
        finishOperation();
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

    // The refused-publish route. Their data key and sealing set are taken exactly as
    // validated; the next save applies the rule to them afresh, so nothing of this
    // session's sealing set needs carrying over.
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

    // Nothing to send unless the rule now seals differently - a device this one
    // approved that their version lacks, or one the server has dropped.
    m_publishAgain = true;
}

void DocumentSession::mergeWith(const Fetched& theirs, const Document& merged)
{
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

    publish();
}

void DocumentSession::raiseConflict(const Fetched& theirs)
{
    // The local file takes this session's work under their sealing set and key, so
    // every later save builds on it. The base stays where it was, so a crash
    // before resolve meets the same conflict on reopen.
    Document mine = m_document;
    mine.setSealingSet(theirs.document.sealingSet());
    if (!writeLocal(mine, theirs.sealed.header, theirs.dataKey))
    {
        emit saveFailed(tr("The ward could not be saved to %1.").arg(m_paths.file));
        return;
    }

    m_conflict = theirs;
    m_timers->stop(SessionTimers::Timer::Retry);
    settleSyncState();
    emit conflictRaised(Conflict{nameOf(theirs.document, theirs.published.device), theirs.published.when});
}

std::optional<DocumentSession::Conflict> DocumentSession::pendingConflict() const
{
    if (!m_conflict.has_value())
    {
        return std::nullopt;
    }
    const Fetched& theirs = m_conflict.value();
    return Conflict{nameOf(theirs.document, theirs.published.device), theirs.published.when};
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
    // adopted sealing set and key - which was sealed on their header, so it never
    // restores anyone they removed.
    Document resolved = resolution == Resolution::KeepTheirs ? theirs.document : m_document;
    resolved.setEmergencyResponse(response);

    bool written = false;
    if (resolved == theirs.document)
    {
        written = writeLocal(theirs);
    }
    else
    {
        // Each document with the key it is sealed by: an edit made while the conflict
        // waited may have rotated this session's key.
        written = resolution == Resolution::KeepMine ? writeLocal(resolved, m_header, m_dataKey)
                                                     : writeLocal(resolved, theirs.sealed.header, theirs.dataKey);
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

void DocumentSession::finishLost(const ServerVersion::LostAccess& lost)
{
    if (m_lost)
    {
        return;
    }

    stopEveryTimer();
    m_lost = true;
    m_saveScheduled = false;
    ServerVersion::forget(m_context.records, m_documentId, m_paths);
    emit accessLost(lost);
}

// --- Sharing ---------------------------------------------------------------------

void DocumentSession::share()
{
    if (isShared() || m_busy || m_lost)
    {
        return;
    }

    QString error;
    if (!flush(&error))
    {
        emit saveFailed(error);
        return;
    }

    // Registered under its existing id, sealed to this device alone.
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

std::optional<FileOpenRouter::InviteLink> DocumentSession::invite(Invite::For purpose, Role role, const QString& label)
{
    if (!isShared() || m_lost)
    {
        return std::nullopt;
    }

    const QByteArray seed = DocumentCrypto::randomBytes(FileOpenRouter::InviteLink::SeedSize);
    const InviteKeys keys = inviteKeys(seed);
    Invite open;
    open.purpose = purpose;
    open.person = m_context.identity.member;
    open.role = role;
    open.label = label;
    open.keys = keys.keys();

    bool added = false;
    edit(Edit::Roster, [&open, &added](Document& document) {
        SealingSet roster = document.sealingSet();
        added = roster.addInvite(open);
        document.setSealingSet(roster);
    });
    if (!added)
    {
        return std::nullopt;
    }

    publishNow();
    return FileOpenRouter::InviteLink{m_documentId, seed};
}

// --- The lease -------------------------------------------------------------------

void DocumentSession::takeLease()
{
    if (m_leaseRequested || m_lost)
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
