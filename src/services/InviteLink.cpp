#include "InviteLink.h"

#include "DocumentConstants.h"
#include "DocumentHeader.h"
#include "HexCodec.h"
#include "UpdateChecker.h"

#include <QObject>

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

Invite InviteLink::invite(Invite::For purpose, const MemberId& maker, Role role, const QString& label) const
{
    Invite open;
    open.purpose = purpose;
    open.maker = maker;
    open.role = role;
    open.label = label;
    open.keys = keys().publicKeys();
    return open;
}

QString InviteLink::toString() const
{
    return QString::fromLatin1(DocumentConstants::InvitePrefix) + documentId + '/'
         + QString::fromLatin1(seed.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString InviteLink::message(const QString& inviterName, const QString& wardName) const
{
    return QObject::tr("%1 has invited you to %2 in Ward Planning. Install it from %3, then open this link:\n%4")
        .arg(inviterName, wardName, UpdateChecker::releasePage().toString(), toString());
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
