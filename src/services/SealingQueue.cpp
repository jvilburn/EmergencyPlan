#include "SealingQueue.h"

#include "DocumentFile.h"

using namespace ServerContract;
using namespace std::placeholders;
using ServerVersion::Fetched;

namespace
{

/// Whether the header lacks a device the rule would add.
bool owesASeal(const DocumentHeader& header, const ServerVersion::Access& access)
{
    for (const DeviceKeys& device : access.approved)
    {
        const Fingerprint fingerprint = DocumentCrypto::fingerprintOf(device.publicKey, device.verifyKey);
        if (access.allowList.contains(fingerprint.toHex()) && !header.entryFor(fingerprint).has_value())
        {
            return true;
        }
    }
    return false;
}

} // namespace

SealingQueue::SealingQueue(const DeviceIdentity& identity,
                           DocumentRecords& records,
                           ServerConnection& server,
                           std::unique_ptr<SessionTimers> timers,
                           std::function<DocumentPaths(const QString&)> pathsFor,
                           QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_records(records)
    , m_server(server)
    , m_timers(std::move(timers))
    , m_pathsFor(pathsFor)
{
    m_timers->onTimeout = std::bind(&SealingQueue::onTimer, this, _1);
}

void SealingQueue::setOpenSession(DocumentSession* session)
{
    m_openSession = session;
}

void SealingQueue::sealEverywhere()
{
    for (const QString& documentId : m_records.documentIds())
    {
        if (m_records.record(documentId).baseVersion.has_value() && !m_pending.contains(documentId))
        {
            m_pending.append(documentId);
        }
    }

    emit pendingChanged();
    processNext();
}

void SealingQueue::onTimer(SessionTimers::Timer timer)
{
    if (timer != SessionTimers::Timer::Retry)
    {
        return;
    }

    m_retryWaiting = false;
    m_deferred.clear();
    processNext();
}

void SealingQueue::processNext()
{
    if (m_busy)
    {
        return;
    }

    for (const QString& documentId : m_pending)
    {
        if (m_deferred.contains(documentId))
        {
            continue;
        }

        m_busy = true;
        m_server.head(documentId, this, std::bind(&SealingQueue::onHead, this, documentId, _1));
        return;
    }
}

void SealingQueue::onHead(const QString& documentId, const Result<Head>& head)
{
    m_busy = false;

    if (head.error == ServerError::AccessLost)
    {
        lose(documentId, head.lost.value());
        return;
    }

    if (!head.ok())
    {
        defer(documentId);
        return;
    }

    const ServerVersion::Access access =
        ServerVersion::accessFor(head.value.value(), m_records.record(documentId), m_records.linkedDevices());

    // A header this build cannot read is not one to seal on top of.
    DocumentHeader header;
    if (DocumentHeader::parse(head.value->header, header) != HeaderParse::Ok)
    {
        finish(documentId);
        return;
    }

    if (!owesASeal(header, access))
    {
        ServerVersion::settleGranted(m_records, documentId, header, access);
        finish(documentId);
        return;
    }

    // The open ward's session reseals on its own local file: its head check takes in
    // this head, and the publish that follows applies the rule to it.
    if (m_openSession && m_openSession->documentId() == documentId)
    {
        m_openSession->checkServer();
        finish(documentId);
        return;
    }

    m_busy = true;
    ServerVersion::fetch(m_identity,
                         m_server,
                         documentId,
                         this,
                         std::bind(&SealingQueue::onFetched, this, documentId, access, _1));
}

void SealingQueue::onFetched(const QString& documentId, const ServerVersion::Access& access, const Fetched& theirs)
{
    m_busy = false;

    switch (theirs.outcome)
    {
    case Fetched::Outcome::Accepted:
        break;
    case Fetched::Outcome::AccessLost:
        lose(documentId, theirs.lost.value());
        return;
    case Fetched::Outcome::CallFailed:
        defer(documentId);
        return;
    case Fetched::Outcome::NeedsNewerVersion:
    case Fetched::Outcome::ServerFault:
        finish(documentId);
        return;
    }

    const ServerVersion::Resealed resealed = ServerVersion::reseal(theirs.document, theirs.dataKey, access);
    const std::optional<DocumentFile::Sealed> sealed =
        DocumentFile::seal(resealed.document, theirs.sealed.header, resealed.dataKey);
    if (!sealed.has_value())
    {
        finish(documentId);
        return;
    }

    m_busy = true;
    m_server.upload(documentId,
                    theirs.version,
                    Upload{sealed.value().header, sealed.value().ciphertext},
                    this,
                    std::bind(&SealingQueue::onUploaded, this, documentId, access, sealed.value().header, _1));
}

void SealingQueue::onUploaded(const QString& documentId,
                              const ServerVersion::Access& access,
                              const DocumentHeader& sealed,
                              const Result<qint64>& result)
{
    m_busy = false;

    switch (result.error)
    {
    case ServerError::None:
        ServerVersion::settleGranted(m_records, documentId, sealed, access);
        m_retryDelay = SessionTimers::FirstRetryMilliseconds;
        finish(documentId);
        return;
    case ServerError::VersionConflict:
    case ServerError::NotOnAllowList:
        // Someone published, or someone was dropped, since the head: start over on
        // the server's newer state.
        again(documentId);
        return;
    case ServerError::AccessLost:
        lose(documentId, result.lost.value());
        return;
    case ServerError::Unreachable:
    case ServerError::RateLimited:
        defer(documentId);
        return;
    default:
        finish(documentId);
        return;
    }
}

void SealingQueue::finish(const QString& documentId)
{
    m_pending.removeAll(documentId);
    m_deferred.remove(documentId);
    emit pendingChanged();
    processNext();
}

void SealingQueue::again(const QString& documentId)
{
    m_deferred.remove(documentId);
    processNext();
}

void SealingQueue::defer(const QString& documentId)
{
    m_deferred.insert(documentId);

    if (!m_retryWaiting)
    {
        m_retryWaiting = true;
        m_timers->start(SessionTimers::Timer::Retry, m_retryDelay);
        m_retryDelay = qMin(m_retryDelay * 2, SessionTimers::LastRetryMilliseconds);
    }

    processNext();
}

void SealingQueue::lose(const QString& documentId, const LostAccess& lost)
{
    m_pending.removeAll(documentId);
    m_deferred.remove(documentId);
    emit pendingChanged();

    if (m_openSession && m_openSession->documentId() == documentId)
    {
        m_openSession->finishLost(lost);
    }
    else
    {
        ServerVersion::forget(m_records, documentId, m_pathsFor(documentId));
        emit accessLost(documentId, lost);
    }

    processNext();
}
