#include "FakeServer.h"

#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "Pairing.h"

using namespace ServerContract;

namespace
{

constexpr qint64 SecondsPerHour = 60 * 60;

std::optional<RecipientEntry> entryByVerifyKey(const DocumentHeader& header, const VerifyKey& key)
{
    for (const RecipientEntry& entry : header.recipients)
    {
        if (entry.verifyKey == key)
        {
            return entry;
        }
    }
    return std::nullopt;
}

QHash<QString, RecipientEntry> byFingerprint(const DocumentHeader& header)
{
    QHash<QString, RecipientEntry> entries;
    for (const RecipientEntry& entry : header.recipients)
    {
        entries.insert(entry.fingerprint.toHex(), entry);
    }
    return entries;
}

QSet<QString> peopleOf(const DocumentHeader& header)
{
    QSet<QString> people;
    for (const RecipientEntry& entry : header.recipients)
    {
        people.insert(entry.member.toHex());
    }
    return people;
}

/// What the allow-list and the chain care about: who, which device, which key, and
/// who is an admin. Two headers with equal membership differ only in content and
/// sealed keys, and the chain rule gives the same answer across either.
QSet<QString> membershipOf(const DocumentHeader& header)
{
    QSet<QString> membership;
    for (const RecipientEntry& entry : header.recipients)
    {
        membership.insert(entry.member.toHex() + QLatin1Char('/') + entry.fingerprint.toHex()
                          + QLatin1Char('/') + HexCodec::encode(entry.verifyKey.bytes));
    }
    for (const MemberId& admin : header.admins)
    {
        membership.insert(QLatin1String("admin/") + admin.toHex());
    }
    return membership;
}

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

// --- Authentication ------------------------------------------------------------

Result<QByteArray> FakeServer::challenge()
{
    if (m_offline)
    {
        return Result<QByteArray>::failure(ServerError::Unreachable);
    }

    const QByteArray challenge = DocumentCrypto::generateDataKey().bytes;
    m_challenges.insert(challenge);
    return Result<QByteArray>::success(challenge);
}

Result<Session> FakeServer::authenticate(const VerifyKey& device,
                                         const QByteArray& challenge,
                                         const QByteArray& signature)
{
    if (m_offline)
    {
        return Result<Session>::failure(ServerError::Unreachable);
    }

    // A challenge is good for one login. Taking it out before checking the
    // signature means a failed attempt spends it too.
    if (!m_challenges.remove(challenge) || !device.isValid()
        || !DocumentCrypto::verify(SignatureTag::Auth, challenge, signature, device))
    {
        return Result<Session>::failure(ServerError::Unauthenticated);
    }

    Session session;
    session.token = randomToken();
    m_sessions.insert(session.token, SessionRecord{device, m_now.addSecs(m_limits.sessionSeconds)});
    return Result<Session>::success(session);
}

// --- Documents -----------------------------------------------------------------

Result<qint64> FakeServer::registerDocument(const Session& session,
                                            const Upload& upload,
                                            const QByteArray& requestSignature)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<qint64>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::RegisterDocument,
                                              upload.header.documentId,
                                              registerBody(upload),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<qint64>::failure(requestCheck);
    }

    if (!withinLimits(m_registrations,
                      device,
                      m_limits.registrationsPerHour,
                      m_limits.registrationsPerKey))
    {
        return Result<qint64>::failure(ServerError::RateLimited);
    }

    const DocumentHeader& header = upload.header;
    if (!HexCodec::isCanonical(header.documentId, DocumentHeader::DocumentIdLength)
        || upload.blob.size() > m_limits.maxBlobBytes)
    {
        return Result<qint64>::failure(ServerError::InvalidRequest);
    }

    if (m_documents.contains(header.documentId))
    {
        return Result<qint64>::failure(ServerError::IdTaken);
    }

    if (DocumentCrypto::payloadHash(upload.blob) != header.payloadHash)
    {
        return Result<qint64>::failure(ServerError::PayloadMismatch);
    }

    // The first header has nothing to link from, so it stands on its own: one
    // person, who is its admin, written by one of that person's devices, and signed.
    if (!DocumentCrypto::isCoherent(header) || peopleOf(header).size() != 1)
    {
        return Result<qint64>::failure(ServerError::ChainRefused);
    }

    const std::optional<RecipientEntry> writer = header.entryFor(header.lastWriter);
    if (!writer.has_value() || header.admins != QList<MemberId>{writer.value().member}
        || !DocumentCrypto::verifyHeader(header))
    {
        return Result<qint64>::failure(ServerError::ChainRefused);
    }

    if (writer.value().verifyKey != device)
    {
        return Result<qint64>::failure(ServerError::InvalidRequest);
    }

    // The registering person may bring their own other devices, each one vouched
    // for by a pairing with the registering device itself. Anyone who has seen a
    // header knows a device's public keys; without this, a stranger could list
    // someone else's device in a look-alike ward and have it appear on their list.
    for (const RecipientEntry& entry : header.recipients)
    {
        if (entry.fingerprint != writer.value().fingerprint && !pairedWith(entry.fingerprint, device))
        {
            return Result<qint64>::failure(ServerError::DeviceNotInvited);
        }
    }

    StoredDocument document;
    document.version = 1;
    document.blob = upload.blob;
    document.history.append(StoredVersion{1, header, true});

    m_documents.insert(header.documentId, document);
    m_registrations[device.bytes].append(m_now);
    return Result<qint64>::success(1);
}

Result<Head> FakeServer::head(const Session& session, const QString& documentId)
{
    VerifyKey device;
    StoredDocument* document = nullptr;
    const ServerError failure = member(session, documentId, device, document);
    if (failure != ServerError::None)
    {
        return Result<Head>::failure(failure);
    }

    Head head;
    head.version = document->version;
    head.keyGeneration = document->current().keyGeneration;
    head.header = document->current();

    if (document->lease.has_value() && document->lease.value().expires > m_now)
    {
        head.leaseHolder = document->lease.value().holder;
    }

    return Result<Head>::success(head);
}

Result<HeaderChain> FakeServer::headers(const Session& session,
                                        const QString& documentId,
                                        qint64 after,
                                        qint64 through)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<HeaderChain>::failure(failure);
    }

    const auto found = m_documents.constFind(documentId);
    if (found == m_documents.constEnd())
    {
        return Result<HeaderChain>::failure(ServerError::UnknownDocument);
    }
    const StoredDocument& document = found.value();

    if (after < 0 || through < 1 || through > document.version || after >= through)
    {
        return Result<HeaderChain>::failure(ServerError::InvalidRequest);
    }

    // A current device reads the chain to any version. A former device reads it
    // only up to and including the header that removed it, which is exactly the
    // evidence it needs and nothing written after.
    qint64 cap = through;
    if (!entryByVerifyKey(document.current(), device).has_value())
    {
        std::optional<qint64> removedAt;
        bool listed = false;
        for (const StoredVersion& stored : document.history)
        {
            if (entryByVerifyKey(stored.header, device).has_value())
            {
                listed = true;
                removedAt.reset();
            }
            else if (listed && !removedAt.has_value())
            {
                removedAt = stored.version;
            }
        }

        if (!removedAt.has_value())
        {
            return Result<HeaderChain>::failure(ServerError::NotAllowListed);
        }

        cap = qMin(through, removedAt.value());
        if (after >= cap)
        {
            return Result<HeaderChain>::failure(ServerError::InvalidRequest);
        }
    }

    // The kept headers newer than the base, then the header of the version asked
    // for. A header whose membership did not change is not kept: verifyLink gives
    // the same answer across it, which is what makes dropping it safe.
    HeaderChain chain;
    for (const StoredVersion& stored : document.history)
    {
        if (stored.version <= after || stored.version > cap)
        {
            continue;
        }

        if (!stored.kept && stored.version != cap)
        {
            continue;
        }

        if (document.withheld.contains(stored.version))
        {
            continue;
        }

        chain.headers.append(document.substituted.value(stored.version, stored.header));
    }

    return Result<HeaderChain>::success(chain);
}

Result<Snapshot> FakeServer::download(const Session& session, const QString& documentId)
{
    VerifyKey device;
    StoredDocument* document = nullptr;
    const ServerError failure = member(session, documentId, device, document);
    if (failure != ServerError::None)
    {
        return Result<Snapshot>::failure(failure);
    }

    // One state, so the header always belongs to the blob beside it.
    Snapshot snapshot;
    snapshot.version = document->version;
    snapshot.header = document->current();
    snapshot.blob = document->blob;
    return Result<Snapshot>::success(snapshot);
}

Result<qint64> FakeServer::upload(const Session& session,
                                  const QString& documentId,
                                  qint64 baseVersion,
                                  const Upload& upload,
                                  const QByteArray& requestSignature)
{
    VerifyKey device;
    StoredDocument* document = nullptr;
    const ServerError failure = member(session, documentId, device, document);
    if (failure != ServerError::None)
    {
        return Result<qint64>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::Upload,
                                              documentId,
                                              uploadBody(baseVersion, upload),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<qint64>::failure(requestCheck);
    }

    if (document->forceConflict)
    {
        document->forceConflict = false;
        return Result<qint64>::failure(ServerError::VersionConflict);
    }

    if (baseVersion != document->version)
    {
        return Result<qint64>::failure(ServerError::VersionConflict);
    }

    // Accepted uploads are what advance the save counter, so they are what is
    // counted: pushing it to its cap takes uploads this limit makes centuries long.
    if (!withinLimits(m_uploads, device, m_limits.uploadsPerHour, 0))
    {
        return Result<qint64>::failure(ServerError::RateLimited);
    }

    const DocumentHeader& current = document->current();
    const DocumentHeader& next = upload.header;

    if (next.documentId != documentId || upload.blob.size() > m_limits.maxBlobBytes)
    {
        return Result<qint64>::failure(ServerError::InvalidRequest);
    }

    if (DocumentCrypto::payloadHash(upload.blob) != next.payloadHash)
    {
        return Result<qint64>::failure(ServerError::PayloadMismatch);
    }

    // The device uploading is the device that wrote it.
    const std::optional<RecipientEntry> uploader = entryByVerifyKey(current, device);
    if (!uploader.has_value() || uploader.value().fingerprint != next.lastWriter)
    {
        return Result<qint64>::failure(ServerError::InvalidRequest);
    }

    // The same rule every client applies, admin rules included. It also keeps the
    // version and key generation from going backwards.
    if (!DocumentCrypto::verifyLink(current, next))
    {
        return Result<qint64>::failure(ServerError::ChainRefused);
    }

    // The chain requires only a rise. The server sees every version, so it alone
    // can bound each one, which keeps any member from pinning the counter at its cap.
    if (next.saveCounter - current.saveCounter > DocumentHeader::MaxSaveCounterStep)
    {
        return Result<qint64>::failure(ServerError::CounterJump);
    }

    const QHash<QString, RecipientEntry> before = byFingerprint(current);
    const QHash<QString, RecipientEntry> after = byFingerprint(next);

    // Everything below decides; nothing changes until every check has passed, so
    // a refused upload leaves requests and history exactly as they were.

    // A device joins only a document it asked to join.
    QStringList consumedRequests;
    for (auto entry = after.constBegin(); entry != after.constEnd(); ++entry)
    {
        if (before.contains(entry.key()))
        {
            continue;
        }

        const auto request = document->requests.constFind(entry.key());
        const bool requested = request != document->requests.constEnd()
            && request.value().status == RequestStatus::Pending
            && request.value().request.member == entry.value().member;

        if (requested)
        {
            consumedRequests.append(entry.key());
            continue;
        }

        if (!linkPermitted(entry.value().fingerprint, entry.value().member, current))
        {
            return Result<qint64>::failure(ServerError::DeviceNotInvited);
        }
    }

    // --- Commit: every change together. ---

    const qint64 version = document->version + 1;
    const bool kept = membershipOf(current) != membershipOf(next);

    document->version = version;
    document->blob = upload.blob;
    document->history.append(StoredVersion{version, next, kept});
    m_uploads[device.bytes].append(m_now);

    for (const QString& fingerprint : consumedRequests)
    {
        document->requests[fingerprint].status = RequestStatus::Granted;
    }

    return Result<qint64>::success(version);
}

// --- Lease ---------------------------------------------------------------------

Result<Lease> FakeServer::takeLease(const Session& session,
                                    const QString& documentId,
                                    const QByteArray& requestSignature)
{
    VerifyKey device;
    StoredDocument* document = nullptr;
    const ServerError failure = member(session, documentId, device, document);
    if (failure != ServerError::None)
    {
        return Result<Lease>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::TakeLease,
                                              documentId,
                                              emptyBody(),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<Lease>::failure(requestCheck);
    }

    const Fingerprint self = entryByVerifyKey(document->current(), device).value().fingerprint;

    if (document->lease.has_value() && document->lease.value().holder != self
        && document->lease.value().expires > m_now)
    {
        return Result<Lease>::failure(ServerError::LeaseHeld);
    }

    document->lease = Lease{self, m_now.addSecs(m_limits.leaseSeconds)};
    return Result<Lease>::success(document->lease.value());
}

Result<Done> FakeServer::releaseLease(const Session& session,
                                      const QString& documentId,
                                      const QByteArray& requestSignature)
{
    VerifyKey device;
    StoredDocument* document = nullptr;
    const ServerError failure = member(session, documentId, device, document);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::ReleaseLease,
                                              documentId,
                                              emptyBody(),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<Done>::failure(requestCheck);
    }

    const Fingerprint self = entryByVerifyKey(document->current(), device).value().fingerprint;

    if (document->lease.has_value() && document->lease.value().expires > m_now
        && document->lease.value().holder != self)
    {
        return Result<Done>::failure(ServerError::LeaseHeld);
    }

    document->lease.reset();
    return Result<Done>::success(Done());
}

// --- Access requests -----------------------------------------------------------

Result<Done> FakeServer::requestAccess(const Session& session,
                                       const QString& documentId,
                                       const AccessRequest& request,
                                       const QByteArray& requestSignature)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::RequestAccess,
                                              documentId,
                                              requestAccessBody(request),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<Done>::failure(requestCheck);
    }

    // The one write open to a non-member, so the one a stranger can repeat.
    if (!withinLimits(m_accessRequests, device, m_limits.requestsPerHour, 0))
    {
        return Result<Done>::failure(ServerError::RateLimited);
    }

    const auto found = m_documents.find(documentId);
    if (found == m_documents.end())
    {
        return Result<Done>::failure(ServerError::UnknownDocument);
    }

    // A device asks for itself, not on another device's behalf.
    if (!request.device.isValid() || request.device.verifyKey() != device
        || !request.member.isValid() || request.displayName.isEmpty()
        || entryByVerifyKey(found.value().current(), device).has_value())
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    // A sealing key is one device's, so a request naming one already in this
    // document or in another device's request is claiming a key it does not hold.
    for (const RecipientEntry& entry : found.value().current().recipients)
    {
        if (entry.publicKey == request.device.publicKey())
        {
            return Result<Done>::failure(ServerError::InvalidRequest);
        }
    }
    for (const StoredRequest& other : found.value().requests)
    {
        if (other.requester != device && other.request.device.publicKey() == request.device.publicKey())
        {
            return Result<Done>::failure(ServerError::InvalidRequest);
        }
    }

    // One request per device: a new one replaces the device's earlier request, so
    // its status is always the one it asked most recently.
    for (auto earlier = found.value().requests.begin(); earlier != found.value().requests.end();)
    {
        if (earlier.value().requester == device)
        {
            earlier = found.value().requests.erase(earlier);
        }
        else
        {
            ++earlier;
        }
    }

    StoredRequest stored;
    stored.request = request;
    stored.requester = device;
    found.value().requests.insert(request.device.fingerprint().toHex(), stored);
    m_accessRequests[device.bytes].append(m_now);
    return Result<Done>::success(Done());
}

Result<QList<AccessRequest>> FakeServer::accessRequests(const Session& session,
                                                        const QString& documentId)
{
    VerifyKey device;
    StoredDocument* document = nullptr;
    const ServerError failure = member(session, documentId, device, document);
    if (failure != ServerError::None)
    {
        return Result<QList<AccessRequest>>::failure(failure);
    }

    QList<AccessRequest> pending;
    for (const StoredRequest& stored : document->requests)
    {
        if (stored.status == RequestStatus::Pending)
        {
            pending.append(stored.request);
        }
    }
    return Result<QList<AccessRequest>>::success(pending);
}

Result<Done> FakeServer::clearAccessRequest(const Session& session,
                                            const QString& documentId,
                                            const Fingerprint& requester,
                                            const QByteArray& requestSignature)
{
    VerifyKey device;
    StoredDocument* document = nullptr;
    const ServerError failure = member(session, documentId, device, document);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::ClearAccessRequest,
                                              documentId,
                                              clearAccessRequestBody(requester),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<Done>::failure(requestCheck);
    }

    const auto stored = document->requests.find(requester.toHex());
    if (stored == document->requests.end() || stored.value().status != RequestStatus::Pending)
    {
        return Result<Done>::failure(ServerError::NotFound);
    }

    stored.value().status = RequestStatus::Withdrawn;
    return Result<Done>::success(Done());
}

Result<RequestStatus> FakeServer::myRequestStatus(const Session& session, const QString& documentId)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<RequestStatus>::failure(failure);
    }

    const auto found = m_documents.constFind(documentId);
    if (found == m_documents.constEnd())
    {
        return Result<RequestStatus>::failure(ServerError::UnknownDocument);
    }

    for (const StoredRequest& stored : found.value().requests)
    {
        if (stored.requester == device)
        {
            return Result<RequestStatus>::success(stored.status);
        }
    }

    return Result<RequestStatus>::failure(ServerError::NotFound);
}

// --- This device ---------------------------------------------------------------

Result<QStringList> FakeServer::myDocuments(const Session& session)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<QStringList>::failure(failure);
    }

    // Every device in a current header got there by registering the document, a
    // granted request, or a completed pairing, so this lists only documents the
    // device genuinely joined.
    QStringList documents;
    for (auto document = m_documents.constBegin(); document != m_documents.constEnd(); ++document)
    {
        if (entryByVerifyKey(document.value().current(), device).has_value())
        {
            documents.append(document.key());
        }
    }
    documents.sort();
    return Result<QStringList>::success(documents);
}

// --- Pairings ------------------------------------------------------------------

Result<QString> FakeServer::startPairing(const Session& session,
                                         const QString& deviceName,
                                         const QByteArray& commitment,
                                         const QByteArray& requestSignature)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<QString>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::StartPairing,
                                              QString(),
                                              startPairingBody(deviceName, commitment),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<QString>::failure(requestCheck);
    }

    if (!withinLimits(m_pairingStarts, device, m_limits.pairingsPerHour, 0))
    {
        return Result<QString>::failure(ServerError::RateLimited);
    }

    if (deviceName.isEmpty() || commitment.isEmpty())
    {
        return Result<QString>::failure(ServerError::InvalidRequest);
    }

    QString code;
    do
    {
        code = Pairing::newPairingCode();
    }
    while (m_pairings.contains(code));

    StoredPairing pairing;
    pairing.state.code = code;
    pairing.state.initiatorDeviceName = deviceName;
    pairing.state.commitment = commitment;
    pairing.state.expires = m_now.addSecs(m_limits.pairingSeconds);
    pairing.initiator = device;

    m_pairings.insert(code, pairing);
    m_pairingStarts[device.bytes].append(m_now);
    return Result<QString>::success(code);
}

Result<PairingState> FakeServer::pairing(const Session& session, const QString& code)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<PairingState>::failure(failure);
    }

    const auto found = m_pairings.find(code);
    if (found == m_pairings.end())
    {
        return Result<PairingState>::failure(ServerError::NotFound);
    }

    StoredPairing& stored = found.value();
    if (stored.state.expires <= m_now)
    {
        return Result<PairingState>::failure(ServerError::Expired);
    }

    // The two parties only. Before anyone has responded, the responder-to-be has
    // to read the commitment in order to keep it, so any member may read until
    // then.
    const bool isInitiator = stored.initiator == device;
    const bool isResponder = stored.responder.has_value() && stored.responder.value() == device;
    const bool mayRespond = !stored.responder.has_value() && isMemberAnywhere(device);

    if (!isInitiator && !isResponder && !mayRespond)
    {
        return Result<PairingState>::failure(ServerError::NotAllowListed);
    }

    if (!stored.scriptedReads.isEmpty())
    {
        return Result<PairingState>::success(stored.scriptedReads.takeFirst());
    }

    return Result<PairingState>::success(stored.state);
}

Result<Done> FakeServer::respondToPairing(const Session& session,
                                          const QString& code,
                                          const DeviceKeys& responder,
                                          const QByteArray& requestSignature)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::RespondToPairing,
                                              QString(),
                                              respondToPairingBody(code, responder),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<Done>::failure(requestCheck);
    }

    const auto found = m_pairings.find(code);
    if (found == m_pairings.end())
    {
        return Result<Done>::failure(ServerError::NotFound);
    }

    StoredPairing& stored = found.value();
    if (stored.state.expires <= m_now)
    {
        return Result<Done>::failure(ServerError::Expired);
    }

    if (!isMemberAnywhere(device))
    {
        return Result<Done>::failure(ServerError::NotAllowListed);
    }

    if (stored.responder.has_value())
    {
        return Result<Done>::failure(ServerError::AlreadyWritten);
    }

    if (!responder.isValid() || responder.verifyKey != device || stored.initiator == device)
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    stored.responder = device;
    stored.state.responder = responder;
    return Result<Done>::success(Done());
}

Result<Done> FakeServer::revealPairing(const Session& session,
                                       const QString& code,
                                       const DeviceKeys& initiator,
                                       const QByteArray& nonce,
                                       const QByteArray& requestSignature)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::RevealPairing,
                                              QString(),
                                              revealPairingBody(code, initiator, nonce),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<Done>::failure(requestCheck);
    }

    const auto found = m_pairings.find(code);
    if (found == m_pairings.end())
    {
        return Result<Done>::failure(ServerError::NotFound);
    }

    StoredPairing& stored = found.value();
    if (stored.state.expires <= m_now)
    {
        return Result<Done>::failure(ServerError::Expired);
    }

    if (stored.initiator != device)
    {
        return Result<Done>::failure(ServerError::NotAllowListed);
    }

    // The initiator reveals only after it has read and kept the responder's keys.
    if (!stored.responder.has_value())
    {
        return Result<Done>::failure(ServerError::OutOfOrder);
    }

    if (stored.state.initiator.has_value())
    {
        return Result<Done>::failure(ServerError::AlreadyWritten);
    }

    if (!initiator.isValid() || initiator.verifyKey != device || nonce.size() != Pairing::NonceSize)
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    stored.state.initiator = initiator;
    stored.state.nonce = nonce;
    return Result<Done>::success(Done());
}

Result<Done> FakeServer::handOver(const Session& session,
                                  const QString& code,
                                  const QByteArray& handover,
                                  const QByteArray& requestSignature)
{
    VerifyKey device;
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }

    const ServerError requestCheck = signedBy(session,
                                              device,
                                              Endpoint::HandOver,
                                              QString(),
                                              handOverBody(code, handover),
                                              requestSignature);
    if (requestCheck != ServerError::None)
    {
        return Result<Done>::failure(requestCheck);
    }

    const auto found = m_pairings.find(code);
    if (found == m_pairings.end())
    {
        return Result<Done>::failure(ServerError::NotFound);
    }

    StoredPairing& stored = found.value();
    if (stored.state.expires <= m_now)
    {
        return Result<Done>::failure(ServerError::Expired);
    }

    if (!stored.responder.has_value() || stored.responder.value() != device)
    {
        return Result<Done>::failure(ServerError::NotAllowListed);
    }

    if (!stored.state.initiator.has_value())
    {
        return Result<Done>::failure(ServerError::OutOfOrder);
    }

    if (!stored.state.handover.isEmpty())
    {
        return Result<Done>::failure(ServerError::AlreadyWritten);
    }

    if (handover.isEmpty())
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    stored.state.handover = handover;

    // A completed pairing is lasting permission to add the initiator's device
    // under the responder's person, in any ward that person belongs to, however
    // long the queue takes to reach them all.
    const DeviceKeys& keys = stored.state.initiator.value();
    m_linkPermissions.append(
        LinkPermission{DocumentCrypto::fingerprintOf(keys.publicKey, keys.verifyKey), device});

    return Result<Done>::success(Done());
}

// --- Development only ----------------------------------------------------------

Result<Done> FakeServer::advanceClock(qint64 seconds)
{
    if (seconds < 0)
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    m_now = m_now.addSecs(seconds);
    return Result<Done>::success(Done());
}

// --- Controls ------------------------------------------------------------------

void FakeServer::forceConflict(const QString& documentId)
{
    // Controls act only on what exists, so a mistyped id in a test cannot create a
    // document the server would then report as taken.
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
        found.value().lease = Lease{holder, m_now.addYears(10)};
    }
}

void FakeServer::withholdHeader(const QString& documentId, qint64 version)
{
    const auto found = m_documents.find(documentId);
    if (found != m_documents.end())
    {
        found.value().withheld.insert(version);
    }
}

void FakeServer::substituteHeader(const QString& documentId,
                                  qint64 version,
                                  const DocumentHeader& replacement)
{
    const auto found = m_documents.find(documentId);
    if (found != m_documents.end())
    {
        found.value().substituted.insert(version, replacement);
    }
}

void FakeServer::scriptPairingReads(const QString& code, const QList<PairingState>& reads)
{
    const auto found = m_pairings.find(code);
    if (found != m_pairings.end())
    {
        found.value().scriptedReads = reads;
    }
}

// --- Private -------------------------------------------------------------------

ServerError FakeServer::authenticated(const Session& session, VerifyKey& device) const
{
    if (m_offline)
    {
        return ServerError::Unreachable;
    }

    const auto record = m_sessions.constFind(session.token);
    if (record == m_sessions.constEnd() || record.value().expires <= m_now)
    {
        return ServerError::Unauthenticated;
    }

    device = record.value().device;
    return ServerError::None;
}

ServerError FakeServer::member(const Session& session,
                               const QString& documentId,
                               VerifyKey& device,
                               StoredDocument*& document)
{
    const ServerError failure = authenticated(session, device);
    if (failure != ServerError::None)
    {
        return failure;
    }

    const auto found = m_documents.find(documentId);
    if (found == m_documents.end() || found.value().history.isEmpty())
    {
        return ServerError::UnknownDocument;
    }

    // The allow-list is the current header's recipients, and nothing else.
    if (!entryByVerifyKey(found.value().current(), device).has_value())
    {
        return ServerError::NotAllowListed;
    }

    document = &found.value();
    return ServerError::None;
}

bool FakeServer::isMemberAnywhere(const VerifyKey& device) const
{
    for (const StoredDocument& document : m_documents)
    {
        if (!document.history.isEmpty() && entryByVerifyKey(document.current(), device).has_value())
        {
            return true;
        }
    }
    return false;
}

bool FakeServer::withinLimits(const QHash<QByteArray, QList<QDateTime>>& log,
                              const VerifyKey& device,
                              int perHour,
                              int total) const
{
    const QList<QDateTime> times = log.value(device.bytes);
    if (total > 0 && times.size() >= total)
    {
        return false;
    }

    int recent = 0;
    for (const QDateTime& time : times)
    {
        if (time.secsTo(m_now) < SecondsPerHour)
        {
            ++recent;
        }
    }
    return recent < perHour;
}

bool FakeServer::linkPermitted(const Fingerprint& device,
                               const MemberId& member,
                               const DocumentHeader& current) const
{
    for (const LinkPermission& permission : m_linkPermissions)
    {
        if (permission.initiator != device)
        {
            continue;
        }

        const std::optional<RecipientEntry> responder = entryByVerifyKey(current, permission.responder);
        if (responder.has_value() && responder.value().member == member)
        {
            return true;
        }
    }
    return false;
}

ServerError FakeServer::signedBy(const Session& session,
                                  const VerifyKey& device,
                                  Endpoint endpoint,
                                  const QString& documentId,
                                  const QByteArray& body,
                                  const QByteArray& signature) const
{
    return requestVerifies(endpoint, session, documentId, body, signature, device)
               ? ServerError::None
               : ServerError::Unauthenticated;
}

bool FakeServer::pairedWith(const Fingerprint& initiator, const VerifyKey& responder) const
{
    for (const LinkPermission& permission : m_linkPermissions)
    {
        if (permission.initiator == initiator && permission.responder == responder)
        {
            return true;
        }
    }
    return false;
}

QString FakeServer::randomToken() const
{
    return HexCodec::encode(DocumentCrypto::generateDataKey().bytes);
}
