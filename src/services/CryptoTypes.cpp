#include "CryptoTypes.h"

#include "HexCodec.h"

#include <QStringList>

#include <sodium.h>

// The header writes the sizes as literals to keep <sodium.h> out of every
// translation unit that sees a key. These are the checks that keep those
// literals honest.
static_assert(PublicKey::Size == crypto_box_PUBLICKEYBYTES);
static_assert(SecretKey::Size == crypto_box_SECRETKEYBYTES);
static_assert(VerifyKey::Size == crypto_sign_PUBLICKEYBYTES);
static_assert(SigningKey::Size == crypto_sign_SECRETKEYBYTES);
static_assert(DataKey::Size == crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
static_assert(Fingerprint::Size >= crypto_generichash_BYTES_MIN);
static_assert(Fingerprint::Size <= crypto_generichash_BYTES_MAX);

namespace
{

constexpr int GroupLength = 4;
constexpr QChar GroupSeparator = QLatin1Char('-');

} // namespace

QString Fingerprint::toHex() const
{
    return HexCodec::encode(bytes);
}

QString Fingerprint::toGroupedHex() const
{
    const QString hex = toHex();

    QStringList groups;
    for (int start = 0; start < hex.size(); start += GroupLength)
    {
        groups.append(hex.mid(start, GroupLength));
    }

    return groups.join(GroupSeparator);
}

Fingerprint Fingerprint::fromHex(const QString& hex)
{
    QString plain = hex;
    plain.remove(GroupSeparator);

    Fingerprint fingerprint;
    fingerprint.bytes = HexCodec::decode(plain, Size);
    return fingerprint;
}

QString MemberId::toHex() const
{
    return HexCodec::encode(bytes);
}

MemberId MemberId::fromHex(const QString& hex)
{
    MemberId member;
    member.bytes = HexCodec::decode(hex, Size);
    return member;
}
