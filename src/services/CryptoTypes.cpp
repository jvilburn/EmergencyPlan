#include "CryptoTypes.h"

#include "HexCodec.h"
#include "StrictJson.h"

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

// Part of the payload and settings formats.
const QLatin1String KeyPublicKey("publicKey");
const QLatin1String KeyVerifyKey("verifyKey");

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
    // What a person types after hearing it read aloud: either case, plain or in
    // groups exactly as toGroupedHex writes them.
    const QString text = hex.trimmed().toLower();

    const int hexLength = Size * 2;
    const int groupedLength = hexLength + hexLength / GroupLength - 1;

    QString plain;
    if (text.size() == hexLength)
    {
        plain = text;
    }
    else if (text.size() == groupedLength)
    {
        for (int index = 0; index < text.size(); ++index)
        {
            const bool separatorHere = (index + 1) % (GroupLength + 1) == 0;
            if (separatorHere != (text.at(index) == GroupSeparator))
            {
                return Fingerprint();
            }
            if (!separatorHere)
            {
                plain.append(text.at(index));
            }
        }
    }

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

QJsonObject DeviceKeys::toJson() const
{
    QJsonObject json;
    json[KeyPublicKey] = HexCodec::encode(publicKey.bytes);
    json[KeyVerifyKey] = HexCodec::encode(verifyKey.bytes);
    return json;
}

std::optional<DeviceKeys> DeviceKeys::fromJson(const QJsonObject& json)
{
    QString publicHex;
    QString verifyHex;
    if (!StrictJson::hasOnlyKeys(json, {KeyPublicKey, KeyVerifyKey})
        || !StrictJson::readString(json, KeyPublicKey, publicHex)
        || !StrictJson::readString(json, KeyVerifyKey, verifyHex))
    {
        return std::nullopt;
    }

    DeviceKeys keys;
    keys.publicKey.bytes = HexCodec::decode(publicHex, PublicKey::Size);
    keys.verifyKey.bytes = HexCodec::decode(verifyHex, VerifyKey::Size);
    if (!keys.isValid())
    {
        return std::nullopt;
    }
    return keys;
}
