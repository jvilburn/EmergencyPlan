#include "ServerTestSupport.h"

using namespace ServerContract;
using namespace std::placeholders;

namespace ServerTestSupport
{

Fingerprint TestDevice::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey);
}

Registration TestDevice::registration() const
{
    return Registration{member, displayName, deviceName, sealing.publicKey};
}

MemberId newMember()
{
    MemberId member;
    member.bytes = DocumentCrypto::randomBytes(MemberId::Size);
    return member;
}

QString newDocumentId()
{
    return QString::fromLatin1(DocumentCrypto::randomBytes(DocumentHeader::DocumentIdLength / 2).toHex());
}

TestDevice newPerson(const QString& displayName)
{
    TestDevice device;
    device.member = newMember();
    device.displayName = displayName;
    device.deviceName = displayName + "'s laptop";
    device.sealing = DocumentCrypto::generateKeypair();
    device.signing = DocumentCrypto::generateSigningKeypair();
    return device;
}

TestDevice newDeviceOf(const TestDevice& person, const QString& deviceName)
{
    TestDevice device = newPerson(person.displayName);
    device.member = person.member;
    device.deviceName = deviceName;
    return device;
}

std::optional<Session> signIn(ServerApi& server, const TestDevice& device)
{
    const Result<QByteArray> challenge = wait<QByteArray>(std::bind(&ServerApi::challenge, &server, _1));
    if (!challenge.ok())
    {
        return std::nullopt;
    }

    const QByteArray signature = DocumentCrypto::signChallenge(challenge.value.value(), device.signing.signingKey);
    const Result<Session> session = wait<Session>(
        std::bind(&ServerApi::authenticate, &server, device.signing.verifyKey, challenge.value.value(), signature, _1));
    return session.value;
}

std::optional<Session> join(ServerApi& server, const TestDevice& device)
{
    const std::optional<Session> session = signIn(server, device);
    if (!session.has_value()
        || !wait<Done>(std::bind(&ServerApi::registerPerson, &server, session.value(), device.registration(), _1)).ok())
    {
        return std::nullopt;
    }
    return session;
}

Upload uploadFor(const QString& documentId, const QList<TestDevice>& devices, qint64 saveCounter)
{
    const DataKey dataKey = DocumentCrypto::generateDataKey();

    Upload upload;
    upload.header.documentId = documentId;
    upload.header.saveCounter = saveCounter;
    upload.header.nonce = DocumentCrypto::generateNonce();
    for (const TestDevice& device : devices)
    {
        upload.header.recipients.append(
            RecipientEntry{device.fingerprint(), DocumentCrypto::sealDataKey(dataKey, device.sealing.publicKey)});
    }
    upload.blob = DocumentCrypto::randomBytes(64);
    return upload;
}

QStringList allowListOf(const Head& head)
{
    QStringList fingerprints;
    for (const Member& member : head.members)
    {
        for (const Device& device : member.person.devices)
        {
            fingerprints.append(device.fingerprint.toHex());
        }
    }
    fingerprints.sort();
    return fingerprints;
}

std::optional<SharedDocument> registerWard(ServerApi& server)
{
    SharedDocument ward;
    ward.id = newDocumentId();
    ward.alice = newPerson("Alice");
    ward.bob = newPerson("Bob");
    ward.carol = newPerson("Carol");

    const std::optional<Session> alice = join(server, ward.alice);
    if (!alice.has_value())
    {
        return std::nullopt;
    }
    ward.aliceSession = alice.value();

    const Result<qint64> version = wait<qint64>(
        std::bind(&ServerApi::registerDocument, &server, ward.aliceSession, uploadFor(ward.id, {ward.alice}, 1), _1));
    if (!version.ok())
    {
        return std::nullopt;
    }
    ward.version = version.value.value();
    return ward;
}

bool grantByRequest(ServerApi& server,
                    const QString& documentId,
                    const Session& admin,
                    const TestDevice& device,
                    const Session& deviceSession,
                    const Resealing& resealing)
{
    if (!wait<Done>(std::bind(&ServerApi::requestAccess, &server, deviceSession, documentId, _1)).ok())
    {
        return false;
    }

    const Result<QList<AccessRequest>> requests =
        wait<QList<AccessRequest>>(std::bind(&ServerApi::accessRequests, &server, admin, documentId, _1));
    if (!requests.ok())
    {
        return false;
    }
    for (const AccessRequest& request : requests.value.value())
    {
        if (request.person.member == device.member)
        {
            return wait<Done>(std::bind(&ServerApi::grant, &server, admin, documentId, request.index, resealing, _1))
                .ok();
        }
    }
    return false;
}

Resealing resealingOf(SharedDocument& ward, const QList<TestDevice>& devices)
{
    ++ward.saveCounter;
    return Resealing{ward.id, ward.version, uploadFor(ward.id, devices, ward.saveCounter)};
}

bool grantInto(ServerApi& server,
               SharedDocument& ward,
               const TestDevice& device,
               const Session& deviceSession,
               const QList<TestDevice>& sealedTo)
{
    if (!grantByRequest(server, ward.id, ward.aliceSession, device, deviceSession, resealingOf(ward, sealedTo)))
    {
        return false;
    }
    ++ward.version;
    return true;
}

std::optional<SharedDocument> wardOfThree(ServerApi& server)
{
    std::optional<SharedDocument> ward = registerWard(server);
    if (!ward.has_value())
    {
        return std::nullopt;
    }

    const std::optional<Session> bob = join(server, ward->bob);
    const std::optional<Session> carol = join(server, ward->carol);
    if (!bob.has_value() || !carol.has_value()
        || !grantInto(server, ward.value(), ward->bob, bob.value(), {ward->alice, ward->bob})
        || !grantInto(server, ward.value(), ward->carol, carol.value(), {ward->alice, ward->bob, ward->carol}))
    {
        return std::nullopt;
    }
    ward->bobSession = bob.value();
    ward->carolSession = carol.value();
    return ward;
}

std::optional<Session> link(ServerApi& server,
                            const TestDevice& newDevice,
                            const Session& existing,
                            const QList<Resealing>& resealings)
{
    const std::optional<Session> session = signIn(server, newDevice);
    if (!session.has_value())
    {
        return std::nullopt;
    }

    const Result<QString> code = wait<QString>(
        std::bind(&ServerApi::startLink, &server, session.value(), newDevice.sealing.publicKey, newDevice.deviceName, _1));
    if (!code.ok()
        || !wait<Done>(std::bind(&ServerApi::approveLink, &server, existing, code.value.value(), resealings, _1)).ok())
    {
        return std::nullopt;
    }
    return session;
}

ServerError publish(ServerApi& server, SharedDocument& ward, const Session& session, const QList<TestDevice>& devices)
{
    ++ward.saveCounter;
    const Result<qint64> version = wait<qint64>(std::bind(&ServerApi::upload,
                                                          &server,
                                                          session,
                                                          ward.id,
                                                          ward.version,
                                                          uploadFor(ward.id, devices, ward.saveCounter),
                                                          _1));
    if (version.ok())
    {
        ward.version = version.value.value();
    }
    return version.error;
}

bool advance(ServerApi& server, SharedDocument& ward, qint64 seconds)
{
    if (!wait<Done>(std::bind(&ServerApi::advanceClock, &server, seconds, _1)).ok())
    {
        return false;
    }

    const std::optional<Session> alice = signIn(server, ward.alice);
    const std::optional<Session> bob = signIn(server, ward.bob);
    const std::optional<Session> carol = signIn(server, ward.carol);
    if (!alice.has_value() || !bob.has_value() || !carol.has_value())
    {
        return false;
    }
    ward.aliceSession = alice.value();
    ward.bobSession = bob.value();
    ward.carolSession = carol.value();
    return true;
}

} // namespace ServerTestSupport
