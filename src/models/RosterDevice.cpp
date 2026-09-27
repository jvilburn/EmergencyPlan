#include "RosterDevice.h"

#include "DocumentCrypto.h"
#include "HexCodec.h"

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
    json["deviceName"] = m_deviceName;
    json["publicKey"] = HexCodec::encode(m_publicKey.bytes);
    json["verifyKey"] = HexCodec::encode(m_verifyKey.bytes);
    return json;
}

std::optional<RosterDevice> RosterDevice::fromJson(const QJsonObject& json)
{
    const QJsonValue name = json.value("deviceName");
    if (!name.isString() || name.toString().isEmpty())
    {
        return std::nullopt;
    }

    const QJsonValue publicKey = json.value("publicKey");
    const QJsonValue verifyKey = json.value("verifyKey");
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
