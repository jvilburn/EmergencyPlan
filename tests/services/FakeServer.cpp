#include "FakeServer.h"

#include "DocumentCrypto.h"
#include "HexCodec.h"

#include <QTimer>

using namespace ServerContract;

namespace
{

constexpr qint64 SecondsPerHour = 60 * 60;

/// The device of a person with this verify key, if it is one of theirs.
const Device* deviceOf(const Person& person, const VerifyKey& key)
{
    for (const Device& device : person.devices)
    {
        if (device.keys.verifyKey == key)
        {
            return &device;
        }
    }
    return nullptr;
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

// --- ServerApi: decided at call time, answered later ---------------------------
//
// Each call changes the server's state when it is made, as a request does when it
// reaches a real server, and its reply waits in the queue for a later turn of the
// event loop.

void FakeServer::challenge(Reply<QByteArray> reply)
{
    answer(decideChallenge(), reply);
}

void FakeServer::authenticate(const VerifyKey& device,
                              const QByteArray& challenge,
                              const QByteArray& signature,
                              Reply<Session> reply)
{
    answer(decideAuthenticate(device, challenge, signature), reply);
}

void FakeServer::registerPerson(const Session& session, const Registration& registration, Reply<Done> reply)
{
    answer(decideRegisterPerson(session, registration), reply);
}

void FakeServer::me(const Session& session, Reply<Me> reply)
{
    answer(decideMe(session), reply);
}

void FakeServer::unlinkDevice(const Session& session, const Fingerprint& device, Reply<Done> reply)
{
    answer(decideUnlinkDevice(session, device), reply);
}

void FakeServer::startLink(const Session& session,
                           const PublicKey& publicKey,
                           const QString& deviceName,
                           Reply<QString> reply)
{
    answer(decideStartLink(session, publicKey, deviceName), reply);
}

void FakeServer::link(const Session& session, const QString& code, Reply<LinkState> reply)
{
    answer(decideLink(session, code), reply);
}

void FakeServer::approveLink(const Session& session, const QString& code, Reply<Done> reply)
{
    answer(decideApproveLink(session, code), reply);
}

void FakeServer::registerDocument(const Session& session, const Upload& upload, Reply<qint64> reply)
{
    answer(decideRegisterDocument(session, upload), reply);
}

void FakeServer::head(const Session& session, const QString& documentId, Reply<Head> reply)
{
    answer(decideHead(session, documentId), reply);
}

void FakeServer::download(const Session& session, const QString& documentId, Reply<Snapshot> reply)
{
    answer(decideDownload(session, documentId), reply);
}

void FakeServer::upload(const Session& session,
                        const QString& documentId,
                        qint64 baseVersion,
                        const Upload& upload,
                        Reply<qint64> reply)
{
    answer(decideUpload(session, documentId, baseVersion, upload), reply);
}

void FakeServer::takeLease(const Session& session, const QString& documentId, Reply<Lease> reply)
{
    answer(decideTakeLease(session, documentId), reply);
}

void FakeServer::releaseLease(const Session& session, const QString& documentId, Reply<Done> reply)
{
    answer(decideReleaseLease(session, documentId), reply);
}

void FakeServer::history(const Session& session,
                         const QString& documentId,
                         qint64 after,
                         Reply<QList<HistoryEntry>> reply)
{
    answer(decideHistory(session, documentId, after), reply);
}

void FakeServer::requestAccess(const Session& session, const QString& documentId, Reply<Done> reply)
{
    answer(decideRequestAccess(session, documentId), reply);
}

void FakeServer::accessRequests(const Session& session, const QString& documentId, Reply<QList<AccessRequest>> reply)
{
    answer(decideAccessRequests(session, documentId), reply);
}

void FakeServer::declineRequest(const Session& session, const QString& documentId, qint64 index, Reply<Done> reply)
{
    answer(decideDeclineRequest(session, documentId, index), reply);
}

void FakeServer::myRequestStatus(const Session& session, const QString& documentId, Reply<RequestStatus> reply)
{
    answer(decideMyRequestStatus(session, documentId), reply);
}

void FakeServer::grant(const Session& session, const QString& documentId, qint64 index, Reply<Done> reply)
{
    answer(decideGrant(session, documentId, index), reply);
}

void FakeServer::removeMember(const Session& session,
                              const QString& documentId,
                              const MemberId& member,
                              Reply<Done> reply)
{
    answer(decideRemoveMember(session, documentId, member), reply);
}

void FakeServer::setRole(const Session& session,
                         const QString& documentId,
                         const MemberId& member,
                         Role role,
                         Reply<Done> reply)
{
    answer(decideSetRole(session, documentId, member, role), reply);
}

void FakeServer::advanceClock(qint64 seconds, Reply<Done> reply)
{
    answer(decideAdvanceClock(seconds), reply);
}

// --- Signing in ----------------------------------------------------------------

Result<QByteArray> FakeServer::decideChallenge()
{
    if (m_offline)
    {
        return Result<QByteArray>::failure(ServerError::Unreachable);
    }

    const QByteArray challenge = DocumentCrypto::generateDataKey().bytes;
    m_challenges.insert(challenge);
    ++m_challengesIssued;
    return Result<QByteArray>::success(challenge);
}

Result<Session> FakeServer::decideAuthenticate(const VerifyKey& device,
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
        || !DocumentCrypto::challengeVerifies(challenge, signature, device))
    {
        return Result<Session>::failure(ServerError::Unauthenticated);
    }

    Session session;
    session.token = randomToken();
    m_sessions.insert(session.token, SessionRecord{device, m_now.addSecs(m_limits.sessionSeconds)});
    return Result<Session>::success(session);
}

// --- This device's person ------------------------------------------------------

Result<Done> FakeServer::decideRegisterPerson(const Session& session, const Registration& registration)
{
    Caller caller;
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }

    // An unlinked device claiming its old person hears why it cannot.
    if (m_unlinkedFrom.value(caller.device.bytes) == registration.member.toHex())
    {
        return Result<Done>::lostAccess(m_unlinked.value(caller.device.bytes));
    }

    if (caller.person != nullptr)
    {
        return caller.person->member == registration.member ? Result<Done>::success(Done())
                                                            : Result<Done>::failure(ServerError::InvalidRequest);
    }

    // A new person, with this device as its first. A device joins an existing person
    // only through a link that person approved.
    if (!registration.member.isValid() || !registration.publicKey.isValid() || registration.displayName.isEmpty()
        || registration.deviceName.isEmpty() || m_people.contains(registration.member.toHex()))
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    Device device;
    device.keys = DeviceKeys{registration.publicKey, caller.device};
    device.fingerprint = DocumentCrypto::fingerprintOf(device.keys.publicKey, device.keys.verifyKey);
    device.name = registration.deviceName;
    device.linkedAt = m_now;

    Person person;
    person.member = registration.member;
    person.displayName = registration.displayName;
    person.devices.append(device);

    m_people.insert(person.member.toHex(), person);
    m_personOfDevice.insert(caller.device.bytes, person.member.toHex());
    m_unlinked.remove(caller.device.bytes);
    m_unlinkedFrom.remove(caller.device.bytes);
    return Result<Done>::success(Done());
}

Result<Me> FakeServer::decideMe(const Session& session)
{
    Caller caller;
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return Result<Me>::failure(failure);
    }
    if (caller.person == nullptr)
    {
        return m_unlinked.contains(caller.device.bytes) ? Result<Me>::lostAccess(m_unlinked.value(caller.device.bytes))
                                                        : Result<Me>::failure(ServerError::InvalidRequest);
    }

    Me me;
    me.person = *caller.person;
    for (auto it = m_documents.constBegin(); it != m_documents.constEnd(); ++it)
    {
        if (it.value().members.contains(caller.person->member.toHex()))
        {
            me.documents.append(it.key());
        }
    }
    me.documents.sort();
    return Result<Me>::success(me);
}

Result<Done> FakeServer::decideUnlinkDevice(const Session& session, const Fingerprint& device)
{
    Caller caller;
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }
    if (caller.person == nullptr)
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    // Only the person's own devices, and never the last: that is leaving.
    QList<Device>& devices = caller.person->devices;
    for (qsizetype index = 0; index < devices.size(); ++index)
    {
        if (devices.at(index).fingerprint != device)
        {
            continue;
        }
        if (devices.size() == 1)
        {
            return Result<Done>::failure(ServerError::InvalidRequest);
        }

        const VerifyKey key = devices.at(index).keys.verifyKey;
        const Device* unlinker = deviceOf(*caller.person, caller.device);
        m_unlinked.insert(key.bytes, LostAccess{LostAccess::How::Unlinked, unlinker->name, m_now});
        m_unlinkedFrom.insert(key.bytes, caller.person->member.toHex());
        releaseLeasesOf(devices.at(index).fingerprint);
        m_personOfDevice.remove(key.bytes);
        devices.removeAt(index);
        return Result<Done>::success(Done());
    }

    return Result<Done>::failure(ServerError::NotFound);
}

// --- Linking -------------------------------------------------------------------

Result<QString> FakeServer::decideStartLink(const Session& session, const PublicKey& publicKey, const QString& deviceName)
{
    Caller caller;
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return Result<QString>::failure(failure);
    }
    if (caller.person != nullptr || !publicKey.isValid() || deviceName.isEmpty())
    {
        return Result<QString>::failure(ServerError::InvalidRequest);
    }
    if (!withinLimits(m_linkStarts, caller.device, m_limits.linksPerHour, 0))
    {
        return Result<QString>::failure(ServerError::RateLimited);
    }

    const QString code = randomToken().left(8).toUpper();
    m_links.insert(code, StoredLink{deviceName, DeviceKeys{publicKey, caller.device}, std::nullopt,
                                    m_now.addSecs(m_limits.linkSeconds)});
    m_linkStarts[caller.device.bytes].append(m_now);
    return Result<QString>::success(code);
}

Result<LinkState> FakeServer::decideLink(const Session& session, const QString& code)
{
    Caller caller;
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return Result<LinkState>::failure(failure);
    }

    const auto found = m_links.constFind(code);
    if (found == m_links.constEnd())
    {
        return Result<LinkState>::failure(ServerError::NotFound);
    }

    // The new device itself, or a device of a person who might approve it.
    const StoredLink& stored = found.value();
    if (stored.keys.verifyKey != caller.device && caller.person == nullptr)
    {
        return Result<LinkState>::failure(ServerError::NotFound);
    }
    if (!stored.approvedAs.has_value() && stored.expires <= m_now)
    {
        return Result<LinkState>::failure(ServerError::Expired);
    }

    LinkState state;
    state.deviceName = stored.deviceName;
    state.keys = stored.keys;
    state.expires = stored.expires;
    if (stored.approvedAs.has_value())
    {
        state.approvedAs = m_people.value(stored.approvedAs.value());
    }
    return Result<LinkState>::success(state);
}

Result<Done> FakeServer::decideApproveLink(const Session& session, const QString& code)
{
    Caller caller;
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }
    if (caller.person == nullptr)
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    const auto found = m_links.find(code);
    if (found == m_links.end())
    {
        return Result<Done>::failure(ServerError::NotFound);
    }

    StoredLink& stored = found.value();
    if (stored.approvedAs.has_value() || m_personOfDevice.contains(stored.keys.verifyKey.bytes))
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }
    if (stored.expires <= m_now)
    {
        return Result<Done>::failure(ServerError::Expired);
    }

    Device device;
    device.keys = stored.keys;
    device.fingerprint = DocumentCrypto::fingerprintOf(device.keys.publicKey, device.keys.verifyKey);
    device.name = stored.deviceName;
    device.linkedBy = caller.fingerprint;
    device.linkedAt = m_now;

    caller.person->devices.append(device);
    m_personOfDevice.insert(device.keys.verifyKey.bytes, caller.person->member.toHex());
    m_unlinked.remove(device.keys.verifyKey.bytes);
    m_unlinkedFrom.remove(device.keys.verifyKey.bytes);
    stored.approvedAs = caller.person->member.toHex();
    return Result<Done>::success(Done());
}

// --- Documents -----------------------------------------------------------------

Result<qint64> FakeServer::decideRegisterDocument(const Session& session, const Upload& upload)
{
    Caller caller;
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return Result<qint64>::failure(failure);
    }
    if (caller.person == nullptr)
    {
        return Result<qint64>::failure(ServerError::InvalidRequest);
    }

    if (!withinLimits(m_registrations, caller.device, m_limits.registrationsPerHour, m_limits.registrationsPerKey))
    {
        return Result<qint64>::failure(ServerError::RateLimited);
    }

    const DocumentHeader& header = upload.header;
    if (!HexCodec::isCanonical(header.documentId, DocumentHeader::DocumentIdLength)
        || upload.blob.size() > m_limits.maxBlobBytes || header.recipients.isEmpty())
    {
        return Result<qint64>::failure(ServerError::InvalidRequest);
    }

    if (m_documents.contains(header.documentId))
    {
        return Result<qint64>::failure(ServerError::IdTaken);
    }

    // A new ward is sealed only to its creator's own devices.
    QSet<QString> own;
    for (const Device& device : caller.person->devices)
    {
        own.insert(device.fingerprint.toHex());
    }
    for (const RecipientEntry& entry : header.recipients)
    {
        if (!own.contains(entry.fingerprint.toHex()))
        {
            return Result<qint64>::failure(ServerError::NotOnAllowList);
        }
    }

    StoredDocument document;
    document.version = 1;
    document.header = header.toJson();
    document.blob = upload.blob;
    document.published = Publication{caller.person->displayName, caller.fingerprint, m_now};
    document.members.insert(caller.person->member.toHex(), Role::Admin);

    m_documents.insert(header.documentId, document);
    m_registrations[caller.device.bytes].append(m_now);
    return Result<qint64>::success(1);
}

Result<Head> FakeServer::decideHead(const Session& session, const QString& documentId)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = allowListed(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<Head>(failure, lost);
    }

    Head head;
    head.version = document->version;
    head.published = document->published;
    head.header = document->servedHeader.value_or(document->header);
    head.members = membersOf(*document);
    for (Member& member : head.members)
    {
        member.person.devices.removeIf(std::bind(&FakeServer::isHidden, this, std::placeholders::_1));
    }
    head.history = document->history.isEmpty() ? 0 : document->history.last().index;
    if (document->lease.has_value() && document->lease.value().expires > m_now)
    {
        head.leaseHolder = document->lease.value().holder;
    }
    return Result<Head>::success(head);
}

Result<Snapshot> FakeServer::decideDownload(const Session& session, const QString& documentId)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = allowListed(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<Snapshot>(failure, lost);
    }

    if (document->servedHeader.has_value())
    {
        return Result<Snapshot>::success(
            Snapshot{document->version, document->published, document->servedHeader.value(), document->servedBlob});
    }
    return Result<Snapshot>::success(Snapshot{document->version, document->published, document->header, document->blob});
}

Result<qint64> FakeServer::decideUpload(const Session& session,
                                        const QString& documentId,
                                        qint64 baseVersion,
                                        const Upload& upload)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = allowListed(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<qint64>(failure, lost);
    }

    if (!withinLimits(m_uploads, caller.device, m_limits.uploadsPerHour, 0))
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

    // Fewer devices than the allow-list is fine - an addition is sealed by its
    // approver's next version - but never one the allow-list has dropped.
    const QSet<QString> allowList = allowListOf(*document);
    for (const RecipientEntry& entry : upload.header.recipients)
    {
        if (!allowList.contains(entry.fingerprint.toHex()))
        {
            return Result<qint64>::failure(ServerError::NotOnAllowList);
        }
    }

    ++document->version;
    document->header = upload.header.toJson();
    document->blob = upload.blob;
    document->published = Publication{caller.person->displayName, caller.fingerprint, m_now};
    m_uploads[caller.device.bytes].append(m_now);
    return Result<qint64>::success(document->version);
}

// --- Lease ---------------------------------------------------------------------

Result<Lease> FakeServer::decideTakeLease(const Session& session, const QString& documentId)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = allowListed(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<Lease>(failure, lost);
    }

    if (document->lease.has_value() && document->lease.value().holder != caller.fingerprint
        && document->lease.value().expires > m_now)
    {
        return Result<Lease>::failure(ServerError::LeaseHeld);
    }

    document->lease = Lease{caller.fingerprint, m_now.addSecs(m_limits.leaseSeconds)};
    return Result<Lease>::success(document->lease.value());
}

Result<Done> FakeServer::decideReleaseLease(const Session& session, const QString& documentId)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = allowListed(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<Done>(failure, lost);
    }

    if (document->lease.has_value() && document->lease.value().expires > m_now
        && document->lease.value().holder != caller.fingerprint)
    {
        return Result<Done>::failure(ServerError::LeaseHeld);
    }

    document->lease.reset();
    return Result<Done>::success(Done());
}

// --- Who has access ------------------------------------------------------------

Result<QList<HistoryEntry>> FakeServer::decideHistory(const Session& session, const QString& documentId, qint64 after)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = allowListed(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<QList<HistoryEntry>>(failure, lost);
    }

    QList<HistoryEntry> entries;
    for (const HistoryEntry& entry : document->history)
    {
        if (entry.index > after)
        {
            entries.append(entry);
        }
    }
    return Result<QList<HistoryEntry>>::success(entries);
}

Result<Done> FakeServer::decideRequestAccess(const Session& session, const QString& documentId)
{
    Caller caller;
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return Result<Done>::failure(failure);
    }
    if (caller.person == nullptr)
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    const auto found = m_documents.find(documentId);
    if (found == m_documents.end() || found.value().members.isEmpty())
    {
        return Result<Done>::failure(ServerError::UnknownDocument);
    }

    StoredDocument& document = found.value();
    if (document.members.contains(caller.person->member.toHex()))
    {
        return Result<Done>::failure(ServerError::InvalidRequest);
    }

    // One pending request per person: asking again changes nothing.
    for (const StoredRequest& stored : document.requests)
    {
        if (stored.status == RequestStatus::Pending && stored.request.person.member == caller.person->member)
        {
            return Result<Done>::success(Done());
        }
    }

    if (!withinLimits(m_accessRequests, caller.device, m_limits.requestsPerHour, 0))
    {
        return Result<Done>::failure(ServerError::RateLimited);
    }

    AccessRequest request;
    request.index = document.requests.size() + 1;
    request.person = *caller.person;
    request.person.devices = {*deviceOf(*caller.person, caller.device)};
    request.when = m_now;

    document.requests.append(StoredRequest{request, RequestStatus::Pending});
    m_accessRequests[caller.device.bytes].append(m_now);
    return Result<Done>::success(Done());
}

Result<QList<AccessRequest>> FakeServer::decideAccessRequests(const Session& session, const QString& documentId)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = allowListed(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<QList<AccessRequest>>(failure, lost);
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

Result<Done> FakeServer::decideDeclineRequest(const Session& session, const QString& documentId, qint64 index)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = admin(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<Done>(failure, lost);
    }

    for (StoredRequest& stored : document->requests)
    {
        if (stored.request.index == index && stored.status == RequestStatus::Pending)
        {
            stored.status = RequestStatus::Declined;
            return Result<Done>::success(Done());
        }
    }
    return Result<Done>::failure(ServerError::NotFound);
}

Result<RequestStatus> FakeServer::decideMyRequestStatus(const Session& session, const QString& documentId)
{
    Caller caller;
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return Result<RequestStatus>::failure(failure);
    }

    const auto found = m_documents.constFind(documentId);
    if (caller.person == nullptr || found == m_documents.constEnd())
    {
        return Result<RequestStatus>::failure(ServerError::NotFound);
    }

    // The latest request this person made.
    for (qsizetype index = found.value().requests.size() - 1; index >= 0; --index)
    {
        const StoredRequest& stored = found.value().requests.at(index);
        if (stored.request.person.member == caller.person->member)
        {
            return Result<RequestStatus>::success(stored.status);
        }
    }
    return Result<RequestStatus>::failure(ServerError::NotFound);
}

Result<Done> FakeServer::decideGrant(const Session& session, const QString& documentId, qint64 index)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = admin(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<Done>(failure, lost);
    }

    for (StoredRequest& stored : document->requests)
    {
        if (stored.request.index != index || stored.status != RequestStatus::Pending)
        {
            continue;
        }

        const QString member = stored.request.person.member.toHex();
        stored.status = RequestStatus::Granted;
        document->members.insert(member, Role::Member);
        document->lost.remove(member);
        record(*document, HistoryEntry::Kind::Added, caller.person->displayName, m_people.value(member).displayName);
        return Result<Done>::success(Done());
    }
    return Result<Done>::failure(ServerError::NotFound);
}

Result<Done> FakeServer::decideRemoveMember(const Session& session, const QString& documentId, const MemberId& member)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = allowListed(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<Done>(failure, lost);
    }

    const QString target = member.toHex();
    const bool leaving = member == caller.person->member;
    if (!leaving && document->members.value(caller.person->member.toHex()) != Role::Admin)
    {
        return Result<Done>::failure(ServerError::NotAdmin);
    }
    if (!document->members.contains(target))
    {
        return Result<Done>::failure(ServerError::NotFound);
    }

    const Role role = document->members.take(target);
    if (!keepsAnAdmin(*document))
    {
        document->members.insert(target, role);
        return Result<Done>::failure(ServerError::LastAdmin);
    }

    for (const Device& device : m_people.value(target).devices)
    {
        if (document->lease.has_value() && document->lease.value().holder == device.fingerprint)
        {
            document->lease.reset();
        }
    }

    const QString subject = m_people.value(target).displayName;
    if (leaving)
    {
        document->lost.insert(target, LostAccess{LostAccess::How::Left, QString(), m_now});
        record(*document, HistoryEntry::Kind::Left, subject, subject);
    }
    else
    {
        document->lost.insert(target, LostAccess{LostAccess::How::Removed, caller.person->displayName, m_now});
        record(*document, HistoryEntry::Kind::Removed, caller.person->displayName, subject);
    }

    // When the last person leaves, nobody can read or write the ward again. What
    // remains is who lost access, so the leaver's other devices still learn why.
    if (document->members.isEmpty())
    {
        document->header = QJsonObject();
        document->blob.clear();
        document->requests.clear();
        document->lease.reset();
    }
    return Result<Done>::success(Done());
}

Result<Done> FakeServer::decideSetRole(const Session& session,
                                       const QString& documentId,
                                       const MemberId& member,
                                       Role role)
{
    Caller caller;
    StoredDocument* document = nullptr;
    std::optional<LostAccess> lost;
    const ServerError failure = admin(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return refused<Done>(failure, lost);
    }

    const QString target = member.toHex();
    if (!document->members.contains(target))
    {
        return Result<Done>::failure(ServerError::NotFound);
    }

    const Role previous = document->members.value(target);
    if (previous == role)
    {
        return Result<Done>::success(Done());
    }

    document->members.insert(target, role);
    if (!keepsAnAdmin(*document))
    {
        document->members.insert(target, previous);
        return Result<Done>::failure(ServerError::LastAdmin);
    }

    record(*document,
           role == Role::Admin ? HistoryEntry::Kind::MadeAdmin : HistoryEntry::Kind::MadeMember,
           caller.person->displayName,
           m_people.value(target).displayName);
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
        found.value().lease = Lease{holder, m_now.addYears(1)};
    }
}

void FakeServer::serveInstead(const QString& documentId, const QJsonObject& header, const QByteArray& blob)
{
    // An empty header goes back to serving what is stored.
    const auto found = m_documents.find(documentId);
    if (found != m_documents.end())
    {
        found.value().servedHeader = header.isEmpty() ? std::nullopt : std::optional<QJsonObject>(header);
        found.value().servedBlob = blob;
    }
}

bool FakeServer::isHidden(const Device& device) const
{
    return m_hidden.contains(device.fingerprint.toHex());
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

ServerError FakeServer::signedIn(const Session& session, Caller& caller)
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

    caller.device = record.value().device;
    const auto person = m_personOfDevice.constFind(caller.device.bytes);
    if (person != m_personOfDevice.constEnd())
    {
        caller.person = &m_people[person.value()];
        caller.fingerprint = deviceOf(*caller.person, caller.device)->fingerprint;
    }
    return ServerError::None;
}

ServerError FakeServer::allowListed(const Session& session,
                                    const QString& documentId,
                                    Caller& caller,
                                    StoredDocument*& document,
                                    std::optional<LostAccess>& lost)
{
    const ServerError failure = signedIn(session, caller);
    if (failure != ServerError::None)
    {
        return failure;
    }

    if (m_unlinked.contains(caller.device.bytes))
    {
        lost = m_unlinked.value(caller.device.bytes);
        return ServerError::AccessLost;
    }

    const auto found = m_documents.find(documentId);
    if (found == m_documents.end())
    {
        return ServerError::UnknownDocument;
    }

    if (caller.person == nullptr)
    {
        return ServerError::NotMember;
    }

    const QString member = caller.person->member.toHex();
    if (!found.value().members.contains(member))
    {
        if (found.value().lost.contains(member))
        {
            lost = found.value().lost.value(member);
            return ServerError::AccessLost;
        }
        return ServerError::NotMember;
    }

    document = &found.value();
    return ServerError::None;
}

ServerError FakeServer::admin(const Session& session,
                              const QString& documentId,
                              Caller& caller,
                              StoredDocument*& document,
                              std::optional<LostAccess>& lost)
{
    const ServerError failure = allowListed(session, documentId, caller, document, lost);
    if (failure != ServerError::None)
    {
        return failure;
    }
    return document->members.value(caller.person->member.toHex()) == Role::Admin ? ServerError::None
                                                                                  : ServerError::NotAdmin;
}

QSet<QString> FakeServer::allowListOf(const StoredDocument& document) const
{
    QSet<QString> allowList;
    for (auto it = document.members.constBegin(); it != document.members.constEnd(); ++it)
    {
        for (const Device& device : m_people.value(it.key()).devices)
        {
            allowList.insert(device.fingerprint.toHex());
        }
    }
    return allowList;
}

QList<Member> FakeServer::membersOf(const StoredDocument& document) const
{
    // In member id order, so two heads of the same state compare equal.
    QStringList ids = document.members.keys();
    ids.sort();

    QList<Member> members;
    for (const QString& id : ids)
    {
        members.append(Member{m_people.value(id), document.members.value(id)});
    }
    return members;
}

void FakeServer::releaseLeasesOf(const Fingerprint& device)
{
    for (StoredDocument& document : m_documents)
    {
        if (document.lease.has_value() && document.lease.value().holder == device)
        {
            document.lease.reset();
        }
    }
}

bool FakeServer::keepsAnAdmin(const StoredDocument& document) const
{
    if (document.members.isEmpty())
    {
        return true;
    }
    for (const Role role : document.members)
    {
        if (role == Role::Admin)
        {
            return true;
        }
    }
    return false;
}

void FakeServer::record(StoredDocument& document,
                        HistoryEntry::Kind kind,
                        const QString& actor,
                        const QString& subject)
{
    document.history.append(HistoryEntry{document.history.size() + 1, kind, actor, subject, m_now});
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

QString FakeServer::randomToken() const
{
    return HexCodec::encode(DocumentCrypto::generateDataKey().bytes);
}
