#include "WardFile.h"
#include "Crypto.h"
#include "JsonReader.h"

#include <QJsonDocument>
#include <QSet>

namespace
{

WardRead unreadable(const QString& problem, const std::optional<WardHeader>& header)
{
    return WardRead{WardReadStatus::Unreadable, problem, header, QByteArray(), std::nullopt};
}

/// Every recipient entry is computed from exactly one device's or invite's pair of public keys
/// in access, and each of those has exactly one entry.
bool headerMatches(const WardHeader& header, const AccessList& access)
{
    QList<QByteArray> expected;
    const QList<RecipientKeys> recipients = access.recipients();
    for (const RecipientKeys& keys : recipients)
    {
        expected.append(Identity::fingerprintOf(keys.publicKey, keys.verifyKey));
    }
    QList<QByteArray> named;
    for (const Recipient& recipient : header.recipients())
    {
        named.append(recipient.fingerprint);
    }
    QSet<QByteArray> expectedSet(expected.begin(), expected.end());
    QSet<QByteArray> namedSet(named.begin(), named.end());
    return expectedSet.size() == expected.size()
        && namedSet.size() == named.size()
        && expectedSet == namedSet;
}

}  // namespace

QByteArray WardFile::write(const DocumentId& document, const VersionRank& version, const QByteArray& dataKey,
                           const WardPayload& payload)
{
    WardHeader header = WardHeader::build(document, version, dataKey, payload.ward.access().recipients());
    QByteArray json = QJsonDocument(payload.toJson()).toJson(QJsonDocument::Compact);
    return header.line() + '\n' + Crypto::encrypt(qCompress(json), header.line(), header.nonce(), dataKey);
}

WardRead WardFile::read(const QByteArray& bytes, const Identity& reader, PayloadKind kind)
{
    qsizetype boundary = bytes.indexOf('\n');
    if (boundary < 0)
    {
        return unreadable("the file has no header line", std::nullopt);
    }
    QByteArray line = bytes.left(boundary);
    HeaderRead headerRead = WardHeader::parse(line);
    switch (headerRead.status)
    {
    case HeaderStatus::Read:
        break;
    case HeaderStatus::Newer:
        return WardRead{WardReadStatus::Newer, QString(), std::nullopt, QByteArray(), std::nullopt};
    case HeaderStatus::Unreadable:
        return unreadable(headerRead.problem, std::nullopt);
    }
    const WardHeader& header = *headerRead.header;

    if (header.recipients().isEmpty())
    {
        return WardRead{WardReadStatus::Deleted, QString(), header, QByteArray(), std::nullopt};
    }
    std::optional<QByteArray> sealedKey = header.sealedKeyFor(reader.fingerprint());
    if (!sealedKey.has_value())
    {
        return WardRead{WardReadStatus::NotNamed, QString(), header, QByteArray(), std::nullopt};
    }

    std::optional<QByteArray> dataKey = reader.unseal(*sealedKey);
    if (!dataKey.has_value())
    {
        return unreadable("the data key does not unseal", header);
    }
    std::optional<QByteArray> compressed = Crypto::decrypt(bytes.mid(boundary + 1), line, header.nonce(), *dataKey);
    if (!compressed.has_value())
    {
        return unreadable("the payload does not decrypt", header);
    }
    std::optional<QJsonObject> object = JsonReader::parseObject(qUncompress(*compressed));
    if (!object.has_value())
    {
        return unreadable("the payload is not a compressed JSON object", header);
    }

    // The payload reader works on a copy of json; the copies share one failure.
    JsonReader json(*object);
    WardPayload payload = WardPayload::fromJson(json, kind);
    if (json.failed())
    {
        return unreadable(json.problem(), header);
    }
    if (!headerMatches(header, payload.ward.access()))
    {
        return unreadable("the header and the access list differ", header);
    }
    return WardRead{WardReadStatus::Read, QString(), header, *dataKey, payload};
}
