#include "RosterDevice.h"

#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "StrictJson.h"

namespace
{

// Part of the payload format.
const QLatin1String KeyDeviceName("deviceName");
const QLatin1String KeyPublicKey("publicKey");
const QLatin1String KeyVerifyKey("verifyKey");

} // namespace

RosterDevice RosterDevice::create(const QString& deviceName,
                                  const PublicKey& publicKey,
                                  const VerifyKey& verifyKey)
{
    RosterDevice device;
    device.m_deviceName = deviceName;
    device.m_publicKey = publicKey;
    device.m_verifyKey = verifyKey;
    device.m_fingerprint = DocumentCrypto::fingerprintOf(publicKey, verifyKey);
    return device;
}

bool RosterDevice::isValid() const
{
    return !m_deviceName.isEmpty() && m_publicKey.isValid() && m_verifyKey.isValid();
}

QJsonObject RosterDevice::toJson() const
{
    QJsonObject json;
    json[KeyDeviceName] = m_deviceName;
    json[KeyPublicKey] = HexCodec::encode(m_publicKey.bytes);
    json[KeyVerifyKey] = HexCodec::encode(m_verifyKey.bytes);
    return json;
}

std::optional<RosterDevice> RosterDevice::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json, {KeyDeviceName, KeyPublicKey, KeyVerifyKey}))
    {
        return std::nullopt;
    }

    const QJsonValue name = json.value(KeyDeviceName);
    if (!name.isString() || name.toString().isEmpty())
    {
        return std::nullopt;
    }

    const QJsonValue publicKey = json.value(KeyPublicKey);
    const QJsonValue verifyKey = json.value(KeyVerifyKey);
    if (!publicKey.isString() || !verifyKey.isString())
    {
        return std::nullopt;
    }

    RosterDevice device;
    device.m_deviceName = name.toString();
    device.m_publicKey.bytes = HexCodec::decode(publicKey.toString(), PublicKey::Size);
    device.m_verifyKey.bytes = HexCodec::decode(verifyKey.toString(), VerifyKey::Size);

    if (!device.isValid())
    {
        return std::nullopt;
    }

    device.m_fingerprint = DocumentCrypto::fingerprintOf(device.m_publicKey, device.m_verifyKey);
    return device;
}

bool RosterDevice::operator==(const RosterDevice& other) const
{
    return m_deviceName == other.m_deviceName
        && m_publicKey == other.m_publicKey
        && m_verifyKey == other.m_verifyKey;
}
