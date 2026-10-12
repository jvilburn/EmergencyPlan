#include "FakeServer.h"

#include "DocumentCrypto.h"

#include <QTimer>

using namespace ServerContract;

namespace
{

constexpr qint64 SecondsPerHour = 60 * 60;

} // namespace

FakeServer::FakeServer()
    : FakeServer(Limits())
{
}

FakeServer::FakeServer(const Limits& limits)
    : m_limits(limits)
    , m_now(QDateTime::fromString("2026-01-05T09:00:00.000Z", Qt::ISODateWithMs))
{
}

// --- ServerApi: decided at call time, answered later ---------------------------
//
// Each call changes the server's state when it is made, as a request does when it
// reaches a real server, and its reply waits in the queue for a later turn of the
// event loop.

void FakeServer::registerDocument(const Signer& signer, const Upload& upload, Reply<qint64> reply)
{
    answer(decideRegisterDocument(signer, upload), reply);
}

void FakeServer::head(const QString& documentId, Reply<Head> reply)
{
    ++m_headsServed;
    answer(decideHead(documentId), reply);
}

void FakeServer::download(const QString& documentId, Reply<Snapshot> reply)
{
    ++m_downloadsServed;
    answer(decideDownload(documentId), reply);
}

void FakeServer::upload(const Signer& signer,
                        const QString& documentId,
                        qint64 baseVersion,
                        const Upload& upload,
                        Reply<qint64> reply)
{
    answer(decideUpload(signer, documentId, baseVersion, upload), reply);
}

void FakeServer::takeLease(const Signer& signer, const QString& documentId, Reply<Lease> reply)
{
    answer(decideTakeLease(signer, documentId), reply);
}

void FakeServer::releaseLease(const Signer& signer, const QString& documentId, Reply<Done> reply)
{
    answer(decideReleaseLease(signer, documentId), reply);
}

void FakeServer::advanceClock(qint64 seconds, Reply<Done> reply)
{
    answer(decideAdvanceClock(seconds), reply);
}

// --- Documents -----------------------------------------------------------------

Result<qint64> FakeServer::decideRegisterDocument(const Signer& signer, const Upload& upload)
{
    if (m_offline)
    {
        return Result<qint64>::failure(ServerError::Unreachable);
    }

    const QString documentId = upload.header.documentId;
    if (documentId.isEmpty() || upload.header.recipients.isEmpty() || upload.blob.size() > m_limits.maxBlobBytes
        || !signer.keys.isValid()
        || !DocumentCrypto::verifies(signedBytes(CallRegister, documentId, 0, upload), signer.signature, signer.keys.verifyKey))
    {
        return Result<qint64>::failure(ServerError::InvalidRequest);
    }
    if (m_documents.contains(documentId))
    {
        return Result<qint64>::failure(ServerError::IdTaken);
    }

    // The first version names its maker; nobody else could have made it readable.
    const QSet<QString> recipients = recipientsOf(upload.header);
    if (!recipients.contains(signer.fingerprint().toHex()))
    {
        return Result<qint64>::failure(ServerError::NotAKeyHolder);
    }

    StoredDocument document;
    document.version = 1;
    document.header = upload.header.toJson();
    document.recipients = recipients;
    document.blob = upload.blob;
    document.published = Publication{signer.fingerprint(), m_now};
    m_documents.insert(documentId, document);
    return Result<qint64>::success(1);
}

Result<Head> FakeServer::decideHead(const QString& documentId)
{
    if (m_offline)
    {
        return Result<Head>::failure(ServerError::Unreachable);
    }
    const auto found = m_documents.constFind(documentId);
    if (found == m_documents.constEnd())
    {
        return Result<Head>::failure(ServerError::UnknownDocument);
    }

    const StoredDocument& document = found.value();
    Head head;
    head.version = document.version;
    head.published = document.published;
    head.header = document.servedHeader.value_or(document.header);
    if (document.lease.has_value() && document.lease.value().expires > m_now)
    {
        head.leaseHolder = document.lease.value().holder;
    }
    return Result<Head>::success(head);
}

Result<Snapshot> FakeServer::decideDownload(const QString& documentId)
{
    if (m_offline)
    {
        return Result<Snapshot>::failure(ServerError::Unreachable);
    }
    const auto found = m_documents.constFind(documentId);
    if (found == m_documents.constEnd())
    {
        return Result<Snapshot>::failure(ServerError::UnknownDocument);
    }

    const StoredDocument& document = found.value();
    if (document.servedHeader.has_value())
    {
        return Result<Snapshot>::success(
            Snapshot{document.version, document.published, document.servedHeader.value(), document.servedBlob});
    }
    return Result<Snapshot>::success(Snapshot{document.version, document.published, document.header, document.blob});
}

Result<qint64> FakeServer::decideUpload(const Signer& signer,
                                        const QString& documentId,
                                        qint64 baseVersion,
                                        const Upload& upload)
{
    StoredDocument* document = nullptr;
    const ServerError failure = keyHolder(signer, documentId, signedBytes(CallUpload, documentId, baseVersion, upload), document);
    if (failure != ServerError::None)
    {
        return Result<qint64>::failure(failure);
    }

    if (!withinLimits(m_uploads, signer.keys.verifyKey, m_limits.uploadsPerHour))
    {
        return Result<qint64>::failure(ServerError::RateLimited);
    }
    if (upload.header.documentId != documentId || upload.blob.size() > m_limits.maxBlobBytes
        || upload.header.recipients.isEmpty())
    {
        return Result<qint64>::failure(ServerError::InvalidRequest);
    }
    if (document->forceConflict || baseVersion != document->version)
    {
        document->forceConflict = false;
        return Result<qint64>::failure(ServerError::VersionConflict);
    }

    ++document->version;
    document->header = upload.header.toJson();
    document->recipients = recipientsOf(upload.header);
    document->blob = upload.blob;
    document->published = Publication{signer.fingerprint(), m_now};
    m_uploads[signer.keys.verifyKey.bytes].append(m_now);
    return Result<qint64>::success(document->version);
}

// --- Lease ---------------------------------------------------------------------

Result<Lease> FakeServer::decideTakeLease(const Signer& signer, const QString& documentId)
{
    StoredDocument* document = nullptr;
    const ServerError failure = keyHolder(signer, documentId, signedBytes(CallTakeLease, documentId), document);
    if (failure != ServerError::None)
    {
        return Result<Lease>::failure(failure);
    }

    if (document->lease.has_value() && document->lease.value().holder != signer.fingerprint()
        && document->lease.value().expires > m_now)
    {
        return Result<Lease>::failure(ServerError::LeaseHeld);
    }

    document->lease = Lease{signer.fingerprint(), m_now.addSecs(m_limits.leaseSeconds)};
    return Result<Lease>::success(document->lease.value());
}

Result<Done> FakeServer::decideReleaseLease(const Signer& signer, const QString& documentId)
{
    StoredDocument* document = nullptr;
    const ServerError failure = keyHolder(signer, documentId, signedBytes(CallReleaseLease, documentId), document);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }

    if (document->lease.has_value() && document->lease.value().expires > m_now
        && document->lease.value().holder != signer.fingerprint())
    {
        return Result<Done>::failure(ServerError::LeaseHeld);
    }

    document->lease.reset();
    return Result<Done>::success(Done());
}

Result<Done> FakeServer::decideAdvanceClock(qint64 seconds)
{
    if (m_offline)
    {
        return Result<Done>::failure(ServerError::Unreachable);
    }
    m_now = m_now.addSecs(seconds);
    return Result<Done>::success(Done());
}

// --- Controls ------------------------------------------------------------------

void FakeServer::forceConflict(const QString& documentId)
{
    const auto found = m_documents.find(documentId);
    if (found != m_documents.end())
    {
        found.value().forceConflict = true;
    }
}

void FakeServer::holdLeaseAs(const QString& documentId, const Fingerprint& holder)
{
    const auto found = m_documents.find(documentId);
    if (found != m_documents.end())
    {
        found.value().lease = Lease{holder, m_now.addSecs(m_limits.leaseSeconds)};
    }
}

void FakeServer::serveInstead(const QString& documentId, const QJsonObject& header, const QByteArray& blob)
{
    const auto found = m_documents.find(documentId);
    if (found == m_documents.end())
    {
        return;
    }
    if (header.isEmpty())
    {
        found.value().servedHeader.reset();
        found.value().servedBlob.clear();
        return;
    }
    found.value().servedHeader = header;
    found.value().servedBlob = blob;
}

void FakeServer::holdReplies(bool hold)
{
    m_holdReplies = hold;
    scheduleDelivery();
}

bool FakeServer::deliverNext()
{
    if (m_replies.isEmpty())
    {
        return false;
    }

    // Taken off the queue before it runs, so a call made from inside the reply
    // queues behind everything already waiting.
    const std::function<void()> reply = m_replies.takeFirst();
    reply();
    return true;
}

// --- Private -------------------------------------------------------------------

void FakeServer::scheduleDelivery()
{
    if (m_holdReplies || m_deliveryScheduled || m_replies.isEmpty())
    {
        return;
    }

    m_deliveryScheduled = true;
    QTimer::singleShot(0, &m_replyContext, std::bind(&FakeServer::deliverWaiting, this));
}

void FakeServer::deliverWaiting()
{
    m_deliveryScheduled = false;

    // Only the replies already waiting: one queued by a call made from inside a
    // reply arrives on a later turn, like any other.
    qsizetype waiting = m_replies.size();
    while (waiting > 0 && !m_holdReplies && deliverNext())
    {
        --waiting;
    }

    scheduleDelivery();
}

ServerError FakeServer::keyHolder(const Signer& signer,
                                  const QString& documentId,
                                  const QByteArray& bytes,
                                  StoredDocument*& document)
{
    if (m_offline)
    {
        return ServerError::Unreachable;
    }
    const auto found = m_documents.find(documentId);
    if (found == m_documents.end())
    {
        return ServerError::UnknownDocument;
    }
    if (!signer.keys.isValid() || !DocumentCrypto::verifies(bytes, signer.signature, signer.keys.verifyKey))
    {
        return ServerError::InvalidRequest;
    }
    if (!found.value().recipients.contains(signer.fingerprint().toHex()))
    {
        return ServerError::NotAKeyHolder;
    }
    document = &found.value();
    return ServerError::None;
}

QSet<QString> FakeServer::recipientsOf(const DocumentHeader& header)
{
    QSet<QString> recipients;
    for (const RecipientEntry& entry : header.recipients)
    {
        recipients.insert(entry.fingerprint.toHex());
    }
    return recipients;
}

bool FakeServer::withinLimits(const QHash<QByteArray, QList<QDateTime>>& log, const VerifyKey& device, int perHour) const
{
    int recent = 0;
    for (const QDateTime& time : log.value(device.bytes))
    {
        if (time.secsTo(m_now) < SecondsPerHour)
        {
            ++recent;
        }
    }
    return recent < perHour;
}
