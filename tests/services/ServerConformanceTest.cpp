#include "ServerConformanceTest.h"

#include "DocumentCrypto.h"
#include "ServerTestSupport.h"

#include <QtTest>

using namespace ServerContract;
using namespace ServerTestSupport;
using namespace std::placeholders;

namespace
{

constexpr qint64 OneHour = 60 * 60;
constexpr qint64 ElevenMinutes = 11 * 60;
constexpr qint64 ThreeMinutes = 3 * 60;

Result<Head> headOf(ServerApi& server, const Session& session, const QString& documentId)
{
    return wait<Head>(std::bind(&ServerApi::head, &server, session, documentId, _1));
}

Result<Snapshot> downloadOf(ServerApi& server, const Session& session, const QString& documentId)
{
    return wait<Snapshot>(std::bind(&ServerApi::download, &server, session, documentId, _1));
}

Result<Me> meOf(ServerApi& server, const Session& session)
{
    return wait<Me>(std::bind(&ServerApi::me, &server, session, _1));
}

Result<Done> removeMember(ServerApi& server, const Session& session, const QString& documentId, const MemberId& member)
{
    return wait<Done>(std::bind(&ServerApi::removeMember, &server, session, documentId, member, _1));
}

Result<Done> setRole(ServerApi& server,
                     const Session& session,
                     const QString& documentId,
                     const MemberId& member,
                     Role role)
{
    return wait<Done>(std::bind(&ServerApi::setRole, &server, session, documentId, member, role, _1));
}

Result<QString> startLink(ServerApi& server, const Session& session, const TestDevice& device)
{
    return wait<QString>(
        std::bind(&ServerApi::startLink, &server, session, device.sealing.publicKey, device.deviceName, _1));
}

Result<Done> approveLink(ServerApi& server,
                         const Session& session,
                         const QString& code,
                         const QList<Resealing>& resealings)
{
    return wait<Done>(std::bind(&ServerApi::approveLink, &server, session, code, resealings, _1));
}

Result<Done> grant(ServerApi& server,
                   const Session& session,
                   const QString& documentId,
                   qint64 index,
                   const Resealing& resealing)
{
    return wait<Done>(std::bind(&ServerApi::grant, &server, session, documentId, index, resealing, _1));
}

Result<LinkState> linkOf(ServerApi& server, const Session& session, const QString& code)
{
    return wait<LinkState>(std::bind(&ServerApi::link, &server, session, code, _1));
}

/// Another ward of Alice's, registered with only her device.
std::optional<SharedDocument> anotherWardOf(ServerApi& server, const SharedDocument& first)
{
    SharedDocument ward = first;
    ward.id = newDocumentId();
    ward.saveCounter = 1;
    const Result<qint64> version = wait<qint64>(
        std::bind(&ServerApi::registerDocument, &server, ward.aliceSession, uploadFor(ward.id, {ward.alice}, 1), _1));
    if (!version.ok())
    {
        return std::nullopt;
    }
    ward.version = version.value.value();
    return ward;
}

Result<Done> unlinkDevice(ServerApi& server, const Session& session, const Fingerprint& device)
{
    return wait<Done>(std::bind(&ServerApi::unlinkDevice, &server, session, device, _1));
}

Result<Lease> takeLease(ServerApi& server, const Session& session, const QString& documentId)
{
    return wait<Lease>(std::bind(&ServerApi::takeLease, &server, session, documentId, _1));
}

Result<Done> releaseLease(ServerApi& server, const Session& session, const QString& documentId)
{
    return wait<Done>(std::bind(&ServerApi::releaseLease, &server, session, documentId, _1));
}

Result<QList<HistoryEntry>> historyOf(ServerApi& server, const Session& session, const QString& documentId, qint64 after)
{
    return wait<QList<HistoryEntry>>(std::bind(&ServerApi::history, &server, session, documentId, after, _1));
}

Result<RequestStatus> statusOf(ServerApi& server, const Session& session, const QString& documentId)
{
    return wait<RequestStatus>(std::bind(&ServerApi::myRequestStatus, &server, session, documentId, _1));
}

Result<QList<AccessRequest>> requestsOf(ServerApi& server, const Session& session, const QString& documentId)
{
    return wait<QList<AccessRequest>>(std::bind(&ServerApi::accessRequests, &server, session, documentId, _1));
}

Result<Done> requestAccess(ServerApi& server, const Session& session, const QString& documentId)
{
    return wait<Done>(std::bind(&ServerApi::requestAccess, &server, session, documentId, _1));
}

std::optional<Member> memberOf(const Head& head, const MemberId& member)
{
    for (const Member& listed : head.members)
    {
        if (listed.person.member == member)
        {
            return listed;
        }
    }
    return std::nullopt;
}

} // namespace

ServerConformanceTest::ServerConformanceTest(std::function<std::unique_ptr<ServerApi>()> makeServer)
    : m_makeServer(std::move(makeServer))
{
}

void ServerConformanceTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void ServerConformanceTest::init()
{
    m_server = m_makeServer();
    QVERIFY(m_server != nullptr);
}

void ServerConformanceTest::cleanup()
{
    m_server.reset();
}

// --- Signing in ------------------------------------------------------------------

void ServerConformanceTest::authenticatesASignedChallenge()
{
    const TestDevice device = newPerson("Alice");
    const std::optional<Session> session = join(server(), device);
    QVERIFY(session.has_value());
    QVERIFY(!session.value().token.isEmpty());
    QVERIFY(meOf(server(), session.value()).ok());
}

void ServerConformanceTest::refusesAChallengeSignedByAnotherKey()
{
    const TestDevice device = newPerson("Alice");
    const TestDevice other = newPerson("Mallory");

    const Result<QByteArray> challenge = wait<QByteArray>(std::bind(&ServerApi::challenge, &server(), _1));
    QVERIFY(challenge.ok());
    const QByteArray signature = DocumentCrypto::signChallenge(challenge.value.value(), other.signing.signingKey);
    const Result<Session> session = wait<Session>(std::bind(
        &ServerApi::authenticate, &server(), device.signing.verifyKey, challenge.value.value(), signature, _1));
    QCOMPARE(session.error, ServerError::Unauthenticated);
}

void ServerConformanceTest::aChallengeIsGoodForOneLogin()
{
    const TestDevice device = newPerson("Alice");
    const Result<QByteArray> challenge = wait<QByteArray>(std::bind(&ServerApi::challenge, &server(), _1));
    QVERIFY(challenge.ok());
    const QByteArray signature = DocumentCrypto::signChallenge(challenge.value.value(), device.signing.signingKey);

    const Result<Session> first = wait<Session>(std::bind(
        &ServerApi::authenticate, &server(), device.signing.verifyKey, challenge.value.value(), signature, _1));
    const Result<Session> second = wait<Session>(std::bind(
        &ServerApi::authenticate, &server(), device.signing.verifyKey, challenge.value.value(), signature, _1));
    QVERIFY(first.ok());
    QCOMPARE(second.error, ServerError::Unauthenticated);
}

void ServerConformanceTest::anExpiredSessionIsUnauthenticated()
{
    const TestDevice device = newPerson("Alice");
    const std::optional<Session> session = join(server(), device);
    QVERIFY(session.has_value());

    QVERIFY(wait<Done>(std::bind(&ServerApi::advanceClock, &server(), 2 * OneHour, _1)).ok());
    QCOMPARE(meOf(server(), session.value()).error, ServerError::Unauthenticated);
    QVERIFY(join(server(), device).has_value());
}

// --- People and their devices ----------------------------------------------------

void ServerConformanceTest::registersANewPersonWithThisDevice()
{
    const TestDevice alice = newPerson("Alice");
    const std::optional<Session> session = join(server(), alice);
    QVERIFY(session.has_value());

    const Result<Me> me = meOf(server(), session.value());
    QVERIFY(me.ok());
    QCOMPARE(me.value->person.member, alice.member);
    QCOMPARE(me.value->person.displayName, QString("Alice"));
    QCOMPARE(me.value->person.devices.size(), 1);
    QCOMPARE(me.value->person.devices.first().fingerprint, alice.fingerprint());
    QCOMPARE(me.value->person.devices.first().name, QString("Alice's laptop"));
    QVERIFY(!me.value->person.devices.first().linkedBy.has_value());
    QVERIFY(me.value->documents.isEmpty());
}

void ServerConformanceTest::registeringAgainChangesNothing()
{
    const TestDevice alice = newPerson("Alice");
    const std::optional<Session> session = join(server(), alice);
    QVERIFY(session.has_value());
    QVERIFY(wait<Done>(std::bind(&ServerApi::registerPerson, &server(), session.value(), alice.registration(), _1)).ok());
    QCOMPARE(meOf(server(), session.value()).value->person.devices.size(), 1);
}

void ServerConformanceTest::aDeviceCannotClaimAnotherPerson()
{
    const TestDevice alice = newPerson("Alice");
    QVERIFY(join(server(), alice).has_value());

    // Alice's member id from a device she never linked.
    TestDevice mallory = newDeviceOf(alice, "Mallory's phone");
    const std::optional<Session> session = signIn(server(), mallory);
    QVERIFY(session.has_value());
    const Result<Done> claimed =
        wait<Done>(std::bind(&ServerApi::registerPerson, &server(), session.value(), mallory.registration(), _1));
    QCOMPARE(claimed.error, ServerError::InvalidRequest);
}

void ServerConformanceTest::anApprovedLinkAddsTheDeviceToThePerson()
{
    const TestDevice laptop = newPerson("Alice");
    const TestDevice phone = newDeviceOf(laptop, "Alice's phone");
    const std::optional<Session> laptopSession = join(server(), laptop);
    const std::optional<Session> phoneSession = signIn(server(), phone);
    QVERIFY(laptopSession.has_value() && phoneSession.has_value());

    const Result<QString> code = startLink(server(), phoneSession.value(), phone);
    QVERIFY(code.ok());

    // The approver reads the new device's name and keys; nobody has approved yet.
    const Result<LinkState> pending =
        wait<LinkState>(std::bind(&ServerApi::link, &server(), laptopSession.value(), code.value.value(), _1));
    QVERIFY(pending.ok());
    QCOMPARE(pending.value->deviceName, QString("Alice's phone"));
    QCOMPARE(pending.value->keys, phone.keys());
    QVERIFY(!pending.value->approvedAs.has_value());

    QVERIFY(approveLink(server(), laptopSession.value(), code.value.value(), {}).ok());

    // The new device learns who it is from the link.
    const Result<LinkState> approved =
        wait<LinkState>(std::bind(&ServerApi::link, &server(), phoneSession.value(), code.value.value(), _1));
    QVERIFY(approved.ok() && approved.value->approvedAs.has_value());
    QCOMPARE(approved.value->approvedAs->member, laptop.member);
    QCOMPARE(approved.value->approvedAs->displayName, QString("Alice"));

    const Result<Me> me = meOf(server(), laptopSession.value());
    QCOMPARE(me.value->person.devices.size(), 2);
    const Device& linked = me.value->person.devices.at(1);
    QCOMPARE(linked.fingerprint, phone.fingerprint());
    QCOMPARE(linked.linkedBy, std::optional<Fingerprint>(laptop.fingerprint()));

    // A second approval of the same code changes nothing more.
    QCOMPARE(approveLink(server(), laptopSession.value(), code.value.value(), {}).error, ServerError::InvalidRequest);
}

void ServerConformanceTest::anApprovalStoresEveryResealingOrNone()
{
    std::optional<SharedDocument> first = registerWard(server());
    QVERIFY(first.has_value());
    std::optional<SharedDocument> second = anotherWardOf(server(), first.value());
    QVERIFY(second.has_value());
    QCOMPARE(publish(server(), first.value(), first->aliceSession, {first->alice}), ServerError::None);
    QCOMPARE(first->version, 2);
    QCOMPARE(second->version, 1);

    const TestDevice phone = newDeviceOf(first->alice, "Alice's phone");
    const std::optional<Session> phoneSession = signIn(server(), phone);
    QVERIFY(phoneSession.has_value());
    const Result<QString> code = startLink(server(), phoneSession.value(), phone);
    QVERIFY(code.ok());

    // The first ward's resealing is built on version 1, one behind: neither lands,
    // and the phone is not added.
    Resealing stale = resealingOf(first.value(), {first->alice, phone});
    stale.baseVersion = 1;
    const Resealing current = resealingOf(second.value(), {second->alice, phone});
    QCOMPARE(approveLink(server(), first->aliceSession, code.value.value(), {current, stale}).error,
             ServerError::VersionConflict);
    QCOMPARE(headOf(server(), first->aliceSession, first->id).value->version, 2);
    QCOMPARE(headOf(server(), first->aliceSession, second->id).value->version, 1);
    QVERIFY(!linkOf(server(), first->aliceSession, code.value.value()).value->approvedAs.has_value());
    QCOMPARE(meOf(server(), first->aliceSession).value->person.devices.size(), 1);
    QCOMPARE(headOf(server(), phoneSession.value(), second->id).error, ServerError::NotMember);

    // Both built on the current versions: both land with the device.
    const Resealing firstResealed = resealingOf(first.value(), {first->alice, phone});
    const Resealing secondResealed = resealingOf(second.value(), {second->alice, phone});
    QVERIFY(approveLink(server(), first->aliceSession, code.value.value(), {firstResealed, secondResealed}).ok());
    QCOMPARE(meOf(server(), first->aliceSession).value->person.devices.size(), 2);

    const Result<Snapshot> firstCopy = downloadOf(server(), phoneSession.value(), first->id);
    QVERIFY(firstCopy.ok());
    QCOMPARE(firstCopy.value->version, 3);
    QCOMPARE(firstCopy.value->header, firstResealed.upload.header.toJson());
    QCOMPARE(firstCopy.value->blob, firstResealed.upload.blob);
    QCOMPARE(firstCopy.value->published.device, first->alice.fingerprint());

    const Result<Snapshot> secondCopy = downloadOf(server(), phoneSession.value(), second->id);
    QVERIFY(secondCopy.ok());
    QCOMPARE(secondCopy.value->version, 2);
    QCOMPARE(secondCopy.value->header, secondResealed.upload.header.toJson());
    QCOMPARE(secondCopy.value->blob, secondResealed.upload.blob);
}

void ServerConformanceTest::anApprovalWithNoResealingsAddsTheDevice()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    const TestDevice phone = newDeviceOf(ward->alice, "Alice's phone");
    const std::optional<Session> phoneSession = signIn(server(), phone);
    QVERIFY(phoneSession.has_value());
    const Result<QString> code = startLink(server(), phoneSession.value(), phone);
    QVERIFY(code.ok());

    QVERIFY(approveLink(server(), ward->aliceSession, code.value.value(), {}).ok());

    // The device is the person's, and so on the ward's allow-list; the ward's version
    // is left as it was.
    QCOMPARE(meOf(server(), ward->aliceSession).value->person.devices.size(), 2);
    const Result<Head> head = headOf(server(), phoneSession.value(), ward->id);
    QVERIFY(head.ok());
    QCOMPARE(head.value->version, 1);
    QCOMPARE(allowListOf(head.value.value()).size(), 2);
    QVERIFY(allowListOf(head.value.value()).contains(phone.fingerprint().toHex()));
}

void ServerConformanceTest::anApprovalNamingAWardTwiceIsRefused()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    const TestDevice phone = newDeviceOf(ward->alice, "Alice's phone");
    const std::optional<Session> phoneSession = signIn(server(), phone);
    QVERIFY(phoneSession.has_value());
    const Result<QString> code = startLink(server(), phoneSession.value(), phone);
    QVERIFY(code.ok());

    // Two resealings of one ward, both on its current version: neither can be
    // checked against what the other would leave, so the call is refused whole.
    const Resealing once = resealingOf(ward.value(), {ward->alice, phone});
    const Resealing twice = resealingOf(ward.value(), {ward->alice, phone});
    QCOMPARE(approveLink(server(), ward->aliceSession, code.value.value(), {once, twice}).error,
             ServerError::InvalidRequest);
    QCOMPARE(headOf(server(), ward->aliceSession, ward->id).value->version, 1);
    QCOMPARE(meOf(server(), ward->aliceSession).value->person.devices.size(), 1);
    QVERIFY(!linkOf(server(), ward->aliceSession, code.value.value()).value->approvedAs.has_value());

    QVERIFY(approveLink(server(), ward->aliceSession, code.value.value(), {once}).ok());
    QCOMPARE(headOf(server(), ward->aliceSession, ward->id).value->version, 2);
}

void ServerConformanceTest::aLinkCodeExpires()
{
    const TestDevice laptop = newPerson("Alice");
    const TestDevice phone = newDeviceOf(laptop, "Alice's phone");
    QVERIFY(join(server(), laptop).has_value());
    const std::optional<Session> phoneSession = signIn(server(), phone);
    const Result<QString> code = startLink(server(), phoneSession.value(), phone);
    QVERIFY(code.ok());

    QVERIFY(wait<Done>(std::bind(&ServerApi::advanceClock, &server(), ElevenMinutes, _1)).ok());
    const std::optional<Session> laptopSession = signIn(server(), laptop);
    QCOMPARE(approveLink(server(), laptopSession.value(), code.value.value(), {}).error, ServerError::Expired);
    QCOMPARE(approveLink(server(), laptopSession.value(), "NOSUCHCODE", {}).error, ServerError::NotFound);
}

void ServerConformanceTest::aDeviceWithAPersonCannotStartALink()
{
    const TestDevice alice = newPerson("Alice");
    const std::optional<Session> session = join(server(), alice);
    QCOMPARE(startLink(server(), session.value(), alice).error, ServerError::InvalidRequest);
}

void ServerConformanceTest::unlinkingCutsTheDeviceOffEveryWard()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    const TestDevice phone = newDeviceOf(ward->alice, "Alice's phone");
    const std::optional<Session> phoneSession = link(server(), phone, ward->aliceSession, {});
    QVERIFY(phoneSession.has_value());
    QVERIFY(headOf(server(), phoneSession.value(), ward->id).ok());

    QVERIFY(unlinkDevice(server(), ward->aliceSession, phone.fingerprint()).ok());

    const Result<Head> refused = headOf(server(), phoneSession.value(), ward->id);
    QCOMPARE(refused.error, ServerError::AccessLost);
    QVERIFY(refused.lost.has_value());
    QCOMPARE(refused.lost->how, LostAccess::How::Unlinked);
    QCOMPARE(refused.lost->byWhom, QString("Alice's laptop"));

    // Its claim to its old person is refused the same way; the laptop is untouched.
    const Result<Done> claim =
        wait<Done>(std::bind(&ServerApi::registerPerson, &server(), phoneSession.value(), phone.registration(), _1));
    QCOMPARE(claim.error, ServerError::AccessLost);
    QCOMPARE(allowListOf(headOf(server(), ward->aliceSession, ward->id).value.value()),
             QStringList{ward->alice.fingerprint().toHex()});
}

void ServerConformanceTest::aPersonsLastDeviceCannotBeUnlinked()
{
    const TestDevice alice = newPerson("Alice");
    const std::optional<Session> session = join(server(), alice);
    QCOMPARE(unlinkDevice(server(), session.value(), alice.fingerprint()).error, ServerError::InvalidRequest);
}

void ServerConformanceTest::anUnlinkedDeviceMayStartOverAsANewPerson()
{
    const TestDevice laptop = newPerson("Alice");
    const std::optional<Session> laptopSession = join(server(), laptop);
    TestDevice phone = newDeviceOf(laptop, "Alice's phone");
    const std::optional<Session> phoneSession = link(server(), phone, laptopSession.value(), {});
    QVERIFY(phoneSession.has_value());
    QVERIFY(unlinkDevice(server(), laptopSession.value(), phone.fingerprint()).ok());

    phone.member = newMember();
    phone.displayName = "Pat";
    const Result<Done> fresh =
        wait<Done>(std::bind(&ServerApi::registerPerson, &server(), phoneSession.value(), phone.registration(), _1));
    QVERIFY(fresh.ok());
    QCOMPARE(meOf(server(), phoneSession.value()).value->person.displayName, QString("Pat"));
}

// --- Documents and versions ------------------------------------------------------

void ServerConformanceTest::registersADocumentWithItsCreatorAsAdmin()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    QCOMPARE(ward->version, 1);

    const Result<Head> head = headOf(server(), ward->aliceSession, ward->id);
    QVERIFY(head.ok());
    QCOMPARE(head.value->version, 1);
    QCOMPARE(head.value->members.size(), 1);
    QCOMPARE(head.value->members.first().person.member, ward->alice.member);
    QCOMPARE(head.value->members.first().role, Role::Admin);
    QCOMPARE(head.value->published.name, QString("Alice"));
    QCOMPARE(head.value->published.device, ward->alice.fingerprint());
    QCOMPARE(head.value->history, 0);
    QVERIFY(!head.value->leaseHolder.has_value());
    QCOMPARE(meOf(server(), ward->aliceSession).value->documents, QStringList{ward->id});
}

void ServerConformanceTest::aNewWardIsSealedOnlyToItsCreatorsDevices()
{
    const TestDevice alice = newPerson("Alice");
    const TestDevice stranger = newPerson("Stranger");
    const std::optional<Session> session = join(server(), alice);
    QVERIFY(join(server(), stranger).has_value());

    const Result<qint64> refused = wait<qint64>(std::bind(
        &ServerApi::registerDocument, &server(), session.value(), uploadFor(newDocumentId(), {alice, stranger}, 1), _1));
    QCOMPARE(refused.error, ServerError::NotOnAllowList);
}

void ServerConformanceTest::refusesATakenDocumentId()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    const TestDevice other = newPerson("Bob");
    const std::optional<Session> session = join(server(), other);

    const Result<qint64> taken = wait<qint64>(
        std::bind(&ServerApi::registerDocument, &server(), session.value(), uploadFor(ward->id, {other}, 1), _1));
    QCOMPARE(taken.error, ServerError::IdTaken);
}

void ServerConformanceTest::writesAreConditionalOnTheStartingVersion()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    QCOMPARE(publish(server(), ward.value(), ward->aliceSession, {ward->alice}), ServerError::None);
    QCOMPARE(ward->version, 2);

    // An upload built on version 1 after version 2 landed.
    const Result<qint64> stale = wait<qint64>(std::bind(
        &ServerApi::upload, &server(), ward->aliceSession, ward->id, 1, uploadFor(ward->id, {ward->alice}, 9), _1));
    QCOMPARE(stale.error, ServerError::VersionConflict);
    QCOMPARE(headOf(server(), ward->aliceSession, ward->id).value->version, 2);
}

void ServerConformanceTest::anUploadListsOnlyAllowListedDevices()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    const TestDevice stranger = newPerson("Stranger");

    QCOMPARE(publish(server(), ward.value(), ward->aliceSession, {ward->alice, stranger}), ServerError::NotOnAllowList);

    // Fewer than the allow-list is fine: a device a version lacks is sealed by the
    // next write of whoever approved it.
    QCOMPARE(publish(server(), ward.value(), ward->aliceSession, {ward->alice}), ServerError::None);
    QCOMPARE(publish(server(), ward.value(), ward->bobSession, {ward->alice, ward->bob, ward->carol}),
             ServerError::None);
}

void ServerConformanceTest::aDownloadIsOneConsistentSnapshot()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    const Upload upload = uploadFor(ward->id, {ward->alice, ward->bob}, 7);
    const Result<qint64> version =
        wait<qint64>(std::bind(&ServerApi::upload, &server(), ward->bobSession, ward->id, ward->version, upload, _1));
    QVERIFY(version.ok());

    const Result<Snapshot> snapshot = downloadOf(server(), ward->carolSession, ward->id);
    QVERIFY(snapshot.ok());
    QCOMPARE(snapshot.value->version, version.value.value());
    QCOMPARE(snapshot.value->header, upload.header.toJson());
    QCOMPARE(snapshot.value->blob, upload.blob);
    QCOMPARE(snapshot.value->published.name, QString("Bob"));
    QCOMPARE(snapshot.value->published.device, ward->bob.fingerprint());
}

// --- Who has access --------------------------------------------------------------

void ServerConformanceTest::onlyMembersReadOrWrite()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    const TestDevice stranger = newPerson("Stranger");
    const std::optional<Session> session = join(server(), stranger);

    QCOMPARE(headOf(server(), session.value(), ward->id).error, ServerError::NotMember);
    QCOMPARE(downloadOf(server(), session.value(), ward->id).error, ServerError::NotMember);
    const Result<qint64> upload = wait<qint64>(
        std::bind(&ServerApi::upload, &server(), session.value(), ward->id, 1, uploadFor(ward->id, {stranger}, 2), _1));
    QCOMPARE(upload.error, ServerError::NotMember);
    QCOMPARE(headOf(server(), session.value(), newDocumentId()).error, ServerError::UnknownDocument);
}

void ServerConformanceTest::aPersonJoinsOnlyThroughAGrantedRequest()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    const std::optional<Session> bob = join(server(), ward->bob);

    QVERIFY(requestAccess(server(), bob.value(), ward->id).ok());
    QCOMPARE(statusOf(server(), bob.value(), ward->id).value, std::optional<RequestStatus>(RequestStatus::Pending));
    QCOMPARE(headOf(server(), bob.value(), ward->id).error, ServerError::NotMember);

    // Asking twice is still one request, listing only the device that asked.
    QVERIFY(requestAccess(server(), bob.value(), ward->id).ok());
    const Result<QList<AccessRequest>> requests = requestsOf(server(), ward->aliceSession, ward->id);
    QCOMPARE(requests.value->size(), 1);
    QCOMPARE(requests.value->first().person.member, ward->bob.member);
    QCOMPARE(requests.value->first().person.displayName, QString("Bob"));
    QCOMPARE(requests.value->first().person.devices.size(), 1);
    QCOMPARE(requests.value->first().person.devices.first().keys, ward->bob.keys());

    QVERIFY(grant(server(),
                  ward->aliceSession,
                  ward->id,
                  requests.value->first().index,
                  resealingOf(ward.value(), {ward->alice, ward->bob}))
                .ok());
    QCOMPARE(statusOf(server(), bob.value(), ward->id).value, std::optional<RequestStatus>(RequestStatus::Granted));

    const Result<Head> head = headOf(server(), bob.value(), ward->id);
    QVERIFY(head.ok());
    QCOMPARE(memberOf(head.value.value(), ward->bob.member)->role, Role::Member);
    QVERIFY(requestsOf(server(), ward->aliceSession, ward->id).value->isEmpty());
}

void ServerConformanceTest::aGrantStoresTheResealedVersionWithTheMembership()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    QCOMPARE(ward->version, 1);
    const std::optional<Session> bob = join(server(), ward->bob);
    QVERIFY(requestAccess(server(), bob.value(), ward->id).ok());
    const qint64 index = requestsOf(server(), ward->aliceSession, ward->id).value->first().index;

    const Resealing resealing = resealingOf(ward.value(), {ward->alice, ward->bob});
    QVERIFY(grant(server(), ward->aliceSession, ward->id, index, resealing).ok());

    const Result<Head> head = headOf(server(), bob.value(), ward->id);
    QVERIFY(head.ok());
    QCOMPARE(head.value->version, 2);
    QCOMPARE(head.value->published.name, QString("Alice"));
    QCOMPARE(head.value->published.device, ward->alice.fingerprint());
    QCOMPARE(memberOf(head.value.value(), ward->bob.member)->role, Role::Member);
    QCOMPARE(statusOf(server(), bob.value(), ward->id).value, std::optional<RequestStatus>(RequestStatus::Granted));

    const Result<Snapshot> snapshot = downloadOf(server(), bob.value(), ward->id);
    QVERIFY(snapshot.ok());
    QCOMPARE(snapshot.value->version, 2);
    QCOMPARE(snapshot.value->header, resealing.upload.header.toJson());
    QCOMPARE(snapshot.value->blob, resealing.upload.blob);
}

void ServerConformanceTest::aGrantOnAStaleVersionChangesNothing()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    QCOMPARE(publish(server(), ward.value(), ward->aliceSession, {ward->alice}), ServerError::None);
    QCOMPARE(ward->version, 2);
    const std::optional<Session> bob = join(server(), ward->bob);
    QVERIFY(requestAccess(server(), bob.value(), ward->id).ok());
    const qint64 index = requestsOf(server(), ward->aliceSession, ward->id).value->first().index;

    Resealing stale = resealingOf(ward.value(), {ward->alice, ward->bob});
    stale.baseVersion = 1;
    QCOMPARE(grant(server(), ward->aliceSession, ward->id, index, stale).error, ServerError::VersionConflict);

    QCOMPARE(statusOf(server(), bob.value(), ward->id).value, std::optional<RequestStatus>(RequestStatus::Pending));
    QCOMPARE(requestsOf(server(), ward->aliceSession, ward->id).value->size(), 1);
    QCOMPARE(headOf(server(), bob.value(), ward->id).error, ServerError::NotMember);
    const Result<Head> head = headOf(server(), ward->aliceSession, ward->id);
    QCOMPARE(head.value->version, 2);
    QCOMPARE(head.value->members.size(), 1);

    // The same request, granted on the current version, lands.
    QVERIFY(grant(server(), ward->aliceSession, ward->id, index, resealingOf(ward.value(), {ward->alice, ward->bob}))
                .ok());
    QCOMPARE(headOf(server(), bob.value(), ward->id).value->version, 3);
}

void ServerConformanceTest::aGrantSealingToAStrangerIsRefused()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    const TestDevice stranger = newPerson("Stranger");
    QVERIFY(join(server(), stranger).has_value());
    const std::optional<Session> bob = join(server(), ward->bob);
    QVERIFY(requestAccess(server(), bob.value(), ward->id).ok());
    const qint64 index = requestsOf(server(), ward->aliceSession, ward->id).value->first().index;

    const Resealing planted = resealingOf(ward.value(), {ward->alice, ward->bob, stranger});
    QCOMPARE(grant(server(), ward->aliceSession, ward->id, index, planted).error, ServerError::NotOnAllowList);

    QCOMPARE(statusOf(server(), bob.value(), ward->id).value, std::optional<RequestStatus>(RequestStatus::Pending));
    const Result<Head> head = headOf(server(), ward->aliceSession, ward->id);
    QCOMPARE(head.value->version, 1);
    QCOMPARE(head.value->members.size(), 1);
    QCOMPARE(head.value->history, 0);

    // Without the stranger, the same grant lands.
    QVERIFY(grant(server(), ward->aliceSession, ward->id, index, resealingOf(ward.value(), {ward->alice, ward->bob}))
                .ok());
    QCOMPARE(headOf(server(), ward->aliceSession, ward->id).value->version, 2);
}

void ServerConformanceTest::everyDeviceOfAMemberIsAllowListed()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    const TestDevice phone = newDeviceOf(ward->bob, "Bob's phone");
    const std::optional<Session> phoneSession = link(server(), phone, ward->bobSession, {});
    QVERIFY(phoneSession.has_value());

    QVERIFY(headOf(server(), phoneSession.value(), ward->id).ok());
    QCOMPARE(publish(server(), ward.value(), ward->aliceSession, {ward->alice, ward->bob, phone}), ServerError::None);
}

void ServerConformanceTest::onlyAnAdminGrantsRemovesOrChangesRoles()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    const TestDevice dave = newPerson("Dave");
    const std::optional<Session> daveSession = join(server(), dave);
    QVERIFY(requestAccess(server(), daveSession.value(), ward->id).ok());
    const qint64 index = requestsOf(server(), ward->aliceSession, ward->id).value->first().index;

    QCOMPARE(grant(server(), ward->bobSession, ward->id, index, resealingOf(ward.value(), {ward->bob, dave})).error,
             ServerError::NotAdmin);
    QCOMPARE(wait<Done>(std::bind(&ServerApi::declineRequest, &server(), ward->bobSession, ward->id, index, _1)).error,
             ServerError::NotAdmin);
    QCOMPARE(removeMember(server(), ward->bobSession, ward->id, ward->carol.member).error, ServerError::NotAdmin);
    QCOMPARE(setRole(server(), ward->bobSession, ward->id, ward->bob.member, Role::Admin).error, ServerError::NotAdmin);

    // Once Alice makes Bob an admin, he may.
    QVERIFY(setRole(server(), ward->aliceSession, ward->id, ward->bob.member, Role::Admin).ok());
    QVERIFY(grant(server(), ward->bobSession, ward->id, index, resealingOf(ward.value(), {ward->bob, dave})).ok());
    QVERIFY(headOf(server(), daveSession.value(), ward->id).ok());
}

void ServerConformanceTest::aDeclinedRequestSaysSo()
{
    std::optional<SharedDocument> ward = registerWard(server());
    const std::optional<Session> bob = join(server(), ward->bob);
    QVERIFY(requestAccess(server(), bob.value(), ward->id).ok());
    const qint64 index = requestsOf(server(), ward->aliceSession, ward->id).value->first().index;

    QVERIFY(wait<Done>(std::bind(&ServerApi::declineRequest, &server(), ward->aliceSession, ward->id, index, _1)).ok());
    QCOMPARE(statusOf(server(), bob.value(), ward->id).value, std::optional<RequestStatus>(RequestStatus::Declined));
    QCOMPARE(headOf(server(), bob.value(), ward->id).error, ServerError::NotMember);
}

void ServerConformanceTest::aRemovalCutsThePersonOffAtOnce()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    QVERIFY(takeLease(server(), ward->bobSession, ward->id).ok());

    QVERIFY(removeMember(server(), ward->aliceSession, ward->id, ward->bob.member).ok());

    const Result<Snapshot> refused = downloadOf(server(), ward->bobSession, ward->id);
    QCOMPARE(refused.error, ServerError::AccessLost);
    QCOMPARE(refused.lost->how, LostAccess::How::Removed);
    QCOMPARE(refused.lost->byWhom, QString("Alice"));

    // A version still sealed to Bob is refused; the remaining two publish without him,
    // and his lease went with him.
    QCOMPARE(publish(server(), ward.value(), ward->carolSession, {ward->alice, ward->bob, ward->carol}),
             ServerError::NotOnAllowList);
    QCOMPARE(publish(server(), ward.value(), ward->carolSession, {ward->alice, ward->carol}), ServerError::None);
    QVERIFY(takeLease(server(), ward->carolSession, ward->id).ok());
}

void ServerConformanceTest::leavingCutsThePersonOffAtOnce()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    QVERIFY(removeMember(server(), ward->carolSession, ward->id, ward->carol.member).ok());

    const Result<Head> refused = headOf(server(), ward->carolSession, ward->id);
    QCOMPARE(refused.error, ServerError::AccessLost);
    QCOMPARE(refused.lost->how, LostAccess::How::Left);
    QVERIFY(refused.lost->byWhom.isEmpty());
    QVERIFY(!memberOf(headOf(server(), ward->aliceSession, ward->id).value.value(), ward->carol.member).has_value());
}

void ServerConformanceTest::theLastAdminCannotLeaveOrStepDown()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());

    QCOMPARE(removeMember(server(), ward->aliceSession, ward->id, ward->alice.member).error, ServerError::LastAdmin);
    QCOMPARE(setRole(server(), ward->aliceSession, ward->id, ward->alice.member, Role::Member).error,
             ServerError::LastAdmin);
    QCOMPARE(memberOf(headOf(server(), ward->aliceSession, ward->id).value.value(), ward->alice.member)->role,
             Role::Admin);

    // With Bob made an admin first, she may do either.
    QVERIFY(setRole(server(), ward->aliceSession, ward->id, ward->bob.member, Role::Admin).ok());
    QVERIFY(setRole(server(), ward->aliceSession, ward->id, ward->alice.member, Role::Member).ok());
    QVERIFY(removeMember(server(), ward->aliceSession, ward->id, ward->alice.member).ok());
}

void ServerConformanceTest::theLastPersonLeavingEndsTheWard()
{
    std::optional<SharedDocument> ward = registerWard(server());
    QVERIFY(ward.has_value());
    const TestDevice phone = newDeviceOf(ward->alice, "Alice's phone");
    const std::optional<Session> phoneSession = link(server(), phone, ward->aliceSession, {});
    QVERIFY(phoneSession.has_value());

    QVERIFY(removeMember(server(), ward->aliceSession, ward->id, ward->alice.member).ok());

    QVERIFY(meOf(server(), ward->aliceSession).value->documents.isEmpty());
    const Result<Head> other = headOf(server(), phoneSession.value(), ward->id);
    QCOMPARE(other.error, ServerError::AccessLost);
    QCOMPARE(other.lost->how, LostAccess::How::Left);

    const std::optional<Session> bob = join(server(), ward->bob);
    QCOMPARE(requestAccess(server(), bob.value(), ward->id).error, ServerError::UnknownDocument);
}

void ServerConformanceTest::aRemovedPersonMayBeGrantedAgain()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    QVERIFY(removeMember(server(), ward->aliceSession, ward->id, ward->bob.member).ok());
    QCOMPARE(headOf(server(), ward->bobSession, ward->id).error, ServerError::AccessLost);

    QVERIFY(grantInto(server(), ward.value(), ward->bob, ward->bobSession, {ward->alice, ward->bob, ward->carol}));
    QVERIFY(headOf(server(), ward->bobSession, ward->id).ok());
}

void ServerConformanceTest::theHistoryListsEachChangeOnce()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    QVERIFY(setRole(server(), ward->aliceSession, ward->id, ward->bob.member, Role::Admin).ok());
    QVERIFY(setRole(server(), ward->bobSession, ward->id, ward->bob.member, Role::Member).ok());
    QVERIFY(removeMember(server(), ward->aliceSession, ward->id, ward->carol.member).ok());

    const Result<Head> head = headOf(server(), ward->aliceSession, ward->id);
    QCOMPARE(head.value->history, 5);

    const Result<QList<HistoryEntry>> all = historyOf(server(), ward->bobSession, ward->id, 0);
    QVERIFY(all.ok());
    QCOMPARE(all.value->size(), 5);
    const QList<HistoryEntry>& entries = all.value.value();
    QCOMPARE(entries.at(0).kind, HistoryEntry::Kind::Added);
    QCOMPARE(entries.at(0).actor, QString("Alice"));
    QCOMPARE(entries.at(0).subject, QString("Bob"));
    QCOMPARE(entries.at(1).subject, QString("Carol"));
    QCOMPARE(entries.at(2).kind, HistoryEntry::Kind::MadeAdmin);
    QCOMPARE(entries.at(3).kind, HistoryEntry::Kind::MadeMember);
    QCOMPARE(entries.at(3).actor, QString("Bob"));
    QCOMPARE(entries.at(3).subject, QString("Bob"));
    QCOMPARE(entries.at(4).kind, HistoryEntry::Kind::Removed);
    QCOMPARE(entries.at(4).subject, QString("Carol"));
    for (qsizetype index = 0; index < entries.size(); ++index)
    {
        QCOMPARE(entries.at(index).index, index + 1);
    }

    const Result<QList<HistoryEntry>> newer = historyOf(server(), ward->bobSession, ward->id, 3);
    QCOMPARE(newer.value->size(), 2);
    QCOMPARE(newer.value->first().index, 4);
}

// --- The lease -------------------------------------------------------------------

void ServerConformanceTest::theLeaseIsHeldByOneDeviceAtATime()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());

    const Result<Lease> taken = takeLease(server(), ward->aliceSession, ward->id);
    QVERIFY(taken.ok());
    QCOMPARE(taken.value->holder, ward->alice.fingerprint());
    QCOMPARE(takeLease(server(), ward->bobSession, ward->id).error, ServerError::LeaseHeld);
    QCOMPARE(releaseLease(server(), ward->bobSession, ward->id).error, ServerError::LeaseHeld);
    QCOMPARE(headOf(server(), ward->bobSession, ward->id).value->leaseHolder,
             std::optional<Fingerprint>(ward->alice.fingerprint()));

    // Refreshed by its holder; taken by another once released.
    QVERIFY(takeLease(server(), ward->aliceSession, ward->id).ok());
    QVERIFY(releaseLease(server(), ward->aliceSession, ward->id).ok());
    QVERIFY(takeLease(server(), ward->bobSession, ward->id).ok());
}

void ServerConformanceTest::aLeaseExpires()
{
    std::optional<SharedDocument> ward = wardOfThree(server());
    QVERIFY(ward.has_value());
    QVERIFY(takeLease(server(), ward->aliceSession, ward->id).ok());

    QVERIFY(advance(server(), ward.value(), ThreeMinutes));
    QVERIFY(!headOf(server(), ward->bobSession, ward->id).value->leaseHolder.has_value());
    QVERIFY(takeLease(server(), ward->bobSession, ward->id).ok());
}
