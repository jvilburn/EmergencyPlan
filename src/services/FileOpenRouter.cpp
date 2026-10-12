#include "FileOpenRouter.h"

#include "DeviceIdentity.h"
#include "DocumentConstants.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "DocumentLibrary.h"
#include "HexCodec.h"
#include "UpdateChecker.h"

#include <QObject>

namespace FileOpenRouter
{

Route route(const QString& path, const DocumentLibrary& library, const DeviceIdentity& identity)
{
    const std::optional<QByteArray> bytes = DocumentFile::readHeader(path);
    if (!bytes.has_value())
    {
        return Route{Action::Unreadable, QString()};
    }

    DocumentFile::Sealed sealed;
    switch (DocumentFile::decode(bytes.value(), sealed))
    {
    case DocumentFile::Read::Ok:
        break;
    case DocumentFile::Read::NeedsNewerVersion:
        return Route{Action::NeedsNewerVersion, QString()};
    case DocumentFile::Read::Malformed:
        return Route{Action::NotAWardPlanningFile, QString()};
    }

    const DocumentHeader& header = sealed.header;
    if (library.contains(header.documentId))
    {
        return Route{Action::OpenWard, header.documentId};
    }
    if (!header.originDocumentId.isEmpty())
    {
        return Route{Action::OpenCopy, QString()};
    }
    if (header.entryFor(identity.fingerprint()).has_value())
    {
        return Route{Action::AddWard, header.documentId};
    }
    return Route{Action::NotForThisDevice, header.documentId};
}

Fingerprint InviteKeys::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey);
}

InviteKeys InviteLink::keys() const
{
    InviteKeys keys;
    keys.sealing = DocumentCrypto::keypairFromSeed(
        DocumentCrypto::hash(HashDomain::InviteSealing, seed, DocumentCrypto::SealingSeedSize));
    keys.signing = DocumentCrypto::signingKeypairFromSeed(
        DocumentCrypto::hash(HashDomain::InviteSigning, seed, DocumentCrypto::SigningSeedSize));
    return keys;
}

QString InviteLink::toString() const
{
    return QString::fromLatin1(DocumentConstants::InvitePrefix) + documentId + '/'
         + QString::fromLatin1(seed.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

std::optional<InviteLink> InviteLink::parse(const QString& argument)
{
    const QString prefix = QString::fromLatin1(DocumentConstants::InvitePrefix);
    if (!argument.startsWith(prefix, Qt::CaseInsensitive))
    {
        return std::nullopt;
    }

    // A browser may add a trailing slash.
    QString rest = argument.mid(prefix.size());
    if (rest.endsWith('/'))
    {
        rest.chop(1);
    }
    const QStringList parts = rest.split('/');
    if (parts.size() != 2 || !HexCodec::isCanonical(parts.at(0), DocumentHeader::DocumentIdLength))
    {
        return std::nullopt;
    }

    const QByteArray::FromBase64Result seed = QByteArray::fromBase64Encoding(
        parts.at(1).toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!seed || seed.decoded.size() != SeedSize)
    {
        return std::nullopt;
    }
    return InviteLink{parts.at(0), seed.decoded};
}

QString inviteMessage(const QString& inviterName, const QString& wardName, const InviteLink& link)
{
    return QObject::tr("%1 has invited you to %2 in Ward Planning. Install it from %3, then open this link:\n%4")
        .arg(inviterName, wardName, UpdateChecker::releasePage().toString(), link.toString());
}

} // namespace FileOpenRouter
