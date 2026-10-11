#include "SealingSet.h"

#include "DocumentCrypto.h"
#include "StrictJson.h"

#include <QJsonArray>

#include <algorithm>

namespace
{

// Part of the payload format.
const QLatin1String KeyDevices("devices");

QString fingerprintHex(const DeviceKeys& device)
{
    return DocumentCrypto::fingerprintOf(device.publicKey, device.verifyKey).toHex();
}

} // namespace

SealingSet SealingSet::of(const DeviceKeys& firstDevice)
{
    SealingSet set;
    set.add(firstDevice);
    return set;
}

bool SealingSet::add(const DeviceKeys& device)
{
    if (!device.isValid())
    {
        return false;
    }

    const QString fingerprint = fingerprintHex(device);
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it)
    {
        if (it.key() != fingerprint
            && (it.value().publicKey == device.publicKey || it.value().verifyKey == device.verifyKey))
        {
            return false;
        }
    }

    m_devices.insert(fingerprint, device);
    return true;
}

bool SealingSet::contains(const Fingerprint& fingerprint) const
{
    return m_devices.contains(fingerprint.toHex());
}

QList<DeviceKeys> SealingSet::devices() const
{
    QStringList fingerprints = m_devices.keys();
    std::sort(fingerprints.begin(), fingerprints.end());

    QList<DeviceKeys> devices;
    for (const QString& fingerprint : fingerprints)
    {
        devices.append(m_devices.value(fingerprint));
    }
    return devices;
}

SealingSet::Next SealingSet::next(const QSet<QString>& allowList, const QList<DeviceKeys>& approved) const
{
    Next next;
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it)
    {
        if (allowList.contains(it.key()))
        {
            next.devices.m_devices.insert(it.key(), it.value());
        }
        else
        {
            next.rotate = true;
        }
    }

    for (const DeviceKeys& device : approved)
    {
        if (allowList.contains(fingerprintHex(device)))
        {
            next.devices.add(device);
        }
    }

    return next;
}

QJsonObject SealingSet::toJson() const
{
    QJsonArray devices;
    for (const DeviceKeys& device : this->devices())
    {
        devices.append(device.toJson());
    }

    QJsonObject json;
    json[KeyDevices] = devices;
    return json;
}

std::optional<SealingSet> SealingSet::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json, {KeyDevices}) || !json.value(KeyDevices).isArray())
    {
        return std::nullopt;
    }

    SealingSet set;
    const QJsonArray devices = json.value(KeyDevices).toArray();
    for (const QJsonValue& value : devices)
    {
        if (!value.isObject())
        {
            return std::nullopt;
        }

        const std::optional<DeviceKeys> device = DeviceKeys::fromJson(value.toObject());
        const int before = set.size();
        if (!device.has_value() || !set.add(device.value()) || set.size() == before)
        {
            return std::nullopt;
        }
    }

    if (set.isEmpty())
    {
        return std::nullopt;
    }
    return set;
}
