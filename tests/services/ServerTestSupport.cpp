#include "ServerTestSupport.h"

using namespace ServerContract;
using namespace std::placeholders;

namespace ServerTestSupport
{

Fingerprint TestDevice::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey);
}

Signer TestDevice::signer(const QByteArray& bytes) const
{
    return Signer{keys(), DocumentCrypto::sign(bytes, signing.signingKey)};
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

TestDevice newDevice(const QString& name)
{
    TestDevice device;
    device.name = name;
    device.sealing = DocumentCrypto::generateKeypair();
    device.signing = DocumentCrypto::generateSigningKeypair();
    return device;
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

Result<qint64> registerAs(ServerApi& server, const TestDevice& device, const Upload& upload)
{
    const Signer signer = device.signer(signedBytes(CallRegister, upload.header.documentId, 0, upload));
    return wait<qint64>(std::bind(&ServerApi::registerDocument, &server, signer, upload, _1));
}

Result<qint64> uploadAs(ServerApi& server,
                        const TestDevice& device,
                        const QString& documentId,
                        qint64 baseVersion,
                        const Upload& upload)
{
    const Signer signer = device.signer(signedBytes(CallUpload, documentId, baseVersion, upload));
    return wait<qint64>(std::bind(&ServerApi::upload, &server, signer, documentId, baseVersion, upload, _1));
}

Result<Lease> takeLeaseAs(ServerApi& server, const TestDevice& device, const QString& documentId)
{
    const Signer signer = device.signer(signedBytes(CallTakeLease, documentId));
    return wait<Lease>(std::bind(&ServerApi::takeLease, &server, signer, documentId, _1));
}

Result<Done> releaseLeaseAs(ServerApi& server, const TestDevice& device, const QString& documentId)
{
    const Signer signer = device.signer(signedBytes(CallReleaseLease, documentId));
    return wait<Done>(std::bind(&ServerApi::releaseLease, &server, signer, documentId, _1));
}

Result<Head> headOf(ServerApi& server, const QString& documentId)
{
    return wait<Head>(std::bind(&ServerApi::head, &server, documentId, _1));
}

Result<Snapshot> downloadOf(ServerApi& server, const QString& documentId)
{
    return wait<Snapshot>(std::bind(&ServerApi::download, &server, documentId, _1));
}

bool advanceClock(ServerApi& server, qint64 seconds)
{
    return wait<Done>(std::bind(&ServerApi::advanceClock, &server, seconds, _1)).ok();
}

QStringList recipientsOf(const Head& head)
{
    DocumentHeader header;
    QStringList fingerprints;
    if (DocumentHeader::parse(head.header, header) != HeaderParse::Ok)
    {
        return fingerprints;
    }
    for (const RecipientEntry& entry : header.recipients)
    {
        fingerprints.append(entry.fingerprint.toHex());
    }
    fingerprints.sort();
    return fingerprints;
}

std::optional<SharedDocument> registerWard(ServerApi& server)
{
    SharedDocument ward;
    ward.id = newDocumentId();
    ward.alice = newDevice("Alice");
    ward.bob = newDevice("Bob");
    ward.carol = newDevice("Carol");

    const Result<qint64> version = registerAs(server, ward.alice, uploadFor(ward.id, {ward.alice}, 1));
    if (!version.ok())
    {
        return std::nullopt;
    }
    ward.version = version.value.value();
    return ward;
}

std::optional<SharedDocument> wardOfThree(ServerApi& server)
{
    std::optional<SharedDocument> ward = registerWard(server);
    if (!ward.has_value()
        || publish(server, ward.value(), ward->alice, {ward->alice, ward->bob, ward->carol}) != ServerError::None)
    {
        return std::nullopt;
    }
    return ward;
}

ServerError publish(ServerApi& server, SharedDocument& ward, const TestDevice& writer, const QList<TestDevice>& devices)
{
    ++ward.saveCounter;
    const Result<qint64> version = uploadAs(server, writer, ward.id, ward.version, uploadFor(ward.id, devices, ward.saveCounter));
    if (version.ok())
    {
        ward.version = version.value.value();
    }
    return version.error;
}

} // namespace ServerTestSupport
