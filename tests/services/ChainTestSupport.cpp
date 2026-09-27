#include "ChainTestSupport.h"

#include <QDateTime>
#include <QStringList>

namespace ChainTestSupport
{

Fingerprint TestDevice::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey);
}

MemberId newMember()
{
    MemberId member;
    member.bytes = DocumentCrypto::generateDataKey().bytes.left(MemberId::Size);
    return member;
}

QString newDocumentId()
{
    return QString::fromLatin1(
        DocumentCrypto::generateDataKey().bytes.left(DocumentHeader::DocumentIdLength / 2).toHex());
}

TestDevice newPerson()
{
    return newDeviceFor(newMember());
}

TestDevice newDeviceFor(const MemberId& member)
{
    TestDevice device;
    device.member = member;
    device.sealing = DocumentCrypto::generateKeypair();
    device.signing = DocumentCrypto::generateSigningKeypair();
    return device;
}

QList<MemberId> canonicalAdmins(const QList<MemberId>& admins)
{
    QStringList hex;
    for (const MemberId& admin : admins)
    {
        if (!hex.contains(admin.toHex()))
        {
            hex.append(admin.toHex());
        }
    }
    hex.sort();

    QList<MemberId> ordered;
    for (const QString& value : hex)
    {
        ordered.append(MemberId::fromHex(value));
    }
    return ordered;
}

DocumentHeader buildHeader(const QString& documentId,
                           int keyGeneration,
                           qint64 saveCounter,
                           const QList<TestDevice>& devices,
                           const TestDevice& writer,
                           const DataKey& dataKey)
{
    QList<MemberId> admins;
    if (!devices.isEmpty())
    {
        admins.append(devices.first().member);
    }
    return buildHeader(documentId, keyGeneration, saveCounter, devices, writer, dataKey, admins);
}

DocumentHeader buildHeader(const QString& documentId,
                           int keyGeneration,
                           qint64 saveCounter,
                           const QList<TestDevice>& devices,
                           const TestDevice& writer,
                           const DataKey& dataKey,
                           const QList<MemberId>& admins)
{
    DocumentHeader header;
    header.documentId = documentId;
    header.keyGeneration = keyGeneration;
    header.saveCounter = saveCounter;
    header.lastWriter = writer.fingerprint();
    header.writerPublicKey = writer.sealing.publicKey;
    header.writerVerifyKey = writer.signing.verifyKey;
    header.lastWriteTime = QDateTime::currentDateTimeUtc();
    header.nonce = DocumentCrypto::generateNonce();
    header.admins = canonicalAdmins(admins);
    header.payloadHash = QByteArray(DocumentCrypto::PayloadHashSize, '\x42');

    for (const TestDevice& device : devices)
    {
        RecipientEntry entry;
        entry.kind = RecipientKind::X25519Sealed;
        entry.member = device.member;
        entry.publicKey = device.sealing.publicKey;
        entry.verifyKey = device.signing.verifyKey;
        entry.fingerprint = device.fingerprint();
        entry.sealedKey = DocumentCrypto::sealDataKey(dataKey, device.sealing.publicKey);
        header.recipients.append(entry);
    }

    resign(header, writer);
    return header;
}

void resign(DocumentHeader& header, const TestDevice& writer)
{
    header.writerSignature = DocumentCrypto::signHeader(header, writer.signing.signingKey);
}

void finishWrite(DocumentHeader& header, const TestDevice& writer)
{
    header.lastWriter = writer.fingerprint();
    header.writerPublicKey = writer.sealing.publicKey;
    header.writerVerifyKey = writer.signing.verifyKey;
    header.saveCounter += 1;
    header.lastWriteTime = QDateTime::currentDateTimeUtc();
    header.nonce = DocumentCrypto::generateNonce();
    header.payloadHash = QByteArray(DocumentCrypto::PayloadHashSize, '\x42');
    resign(header, writer);
}

std::optional<DataKey> unsealFor(const DocumentHeader& header, const TestDevice& device)
{
    const std::optional<RecipientEntry> entry = header.entryFor(device.fingerprint());
    if (!entry.has_value())
    {
        return std::nullopt;
    }

    return DocumentCrypto::unsealDataKey(entry.value().sealedKey, device.sealing);
}

RosterDevice rosterDevice(const TestDevice& device, const QString& name)
{
    return RosterDevice::create(name, device.sealing.publicKey, device.signing.verifyKey);
}

AccessRoster rosterOf(const QList<TestDevice>& devices)
{
    QList<MemberId> admins;
    if (!devices.isEmpty())
    {
        admins.append(devices.first().member);
    }
    return rosterOf(devices, admins);
}

AccessRoster rosterOf(const QList<TestDevice>& devices, const QList<MemberId>& admins)
{
    AccessRoster roster;
    int index = 0;
    for (const TestDevice& device : devices)
    {
        const QString deviceName = QString("Device %1").arg(++index);

        if (roster.contains(device.member))
        {
            const bool added = roster.addDevice(device.member, rosterDevice(device, deviceName));
            Q_ASSERT(added);
            Q_UNUSED(added);
            continue;
        }

        const QString displayName = QString("Person %1").arg(device.member.toHex().left(6));
        const RosterPerson::Role role =
            admins.contains(device.member) ? RosterPerson::Role::Admin : RosterPerson::Role::Member;
        const bool added = roster.addPerson(
            RosterPerson::create(device.member, displayName, rosterDevice(device, deviceName), role));
        Q_ASSERT(added);
        Q_UNUSED(added);
    }
    return roster;
}

} // namespace ChainTestSupport
