#include "RosterPerson.h"

#include "HexCodec.h"
#include "StrictJson.h"

#include <QJsonArray>

#include <algorithm>

namespace
{

// Part of the payload format.
const QLatin1String KeyMember("member");
const QLatin1String KeyDisplayName("displayName");
const QLatin1String KeyRole("role");
const QLatin1String KeyDevices("devices");

const QLatin1String AdminText("admin");
const QLatin1String MemberText("member");

/// Case-insensitive first, so "iPad" sits beside "IPhone" rather than after every
/// capitalised name; then case-sensitive, then fingerprint, so two devices sharing
/// a name still order the same way. Not locale-aware, so the order is the same on
/// every device.
bool deviceSortsBefore(const RosterDevice& left, const RosterDevice& right)
{
    const int byName = QString::compare(left.deviceName(), right.deviceName(), Qt::CaseInsensitive);
    if (byName != 0)
    {
        return byName < 0;
    }
    if (left.deviceName() != right.deviceName())
    {
        return left.deviceName() < right.deviceName();
    }
    return left.fingerprint().toHex() < right.fingerprint().toHex();
}

} // namespace

RosterPerson RosterPerson::create(const MemberId& member,
                                  const QString& displayName,
                                  const RosterDevice& firstDevice,
                                  Role role)
{
    RosterPerson person;
    person.m_member = member;
    person.m_displayName = displayName;
    person.m_role = role;
    person.m_devices.append(firstDevice);
    return person;
}

QList<RosterDevice> RosterPerson::devices() const
{
    QList<RosterDevice> sorted = m_devices;
    std::sort(sorted.begin(), sorted.end(), deviceSortsBefore);
    return sorted;
}

bool RosterPerson::hasDevice(const Fingerprint& fingerprint) const
{
    return deviceFor(fingerprint).has_value();
}

std::optional<RosterDevice> RosterPerson::deviceFor(const Fingerprint& fingerprint) const
{
    if (!fingerprint.isValid())
    {
        return std::nullopt;
    }

    for (const RosterDevice& device : m_devices)
    {
        if (device.fingerprint() == fingerprint)
        {
            return device;
        }
    }

    return std::nullopt;
}

bool RosterPerson::addDevice(const RosterDevice& device)
{
    if (!device.isValid())
    {
        return false;
    }

    const Fingerprint fingerprint = device.fingerprint();
    for (RosterDevice& existing : m_devices)
    {
        if (existing.fingerprint() == fingerprint)
        {
            // Same keys, so the same device. Only the name can have changed.
            existing = device;
            return true;
        }
    }

    m_devices.append(device);
    return true;
}

bool RosterPerson::removeDevice(const Fingerprint& fingerprint)
{
    if (m_devices.size() <= 1)
    {
        return false;
    }

    for (qsizetype index = 0; index < m_devices.size(); ++index)
    {
        if (m_devices.at(index).fingerprint() == fingerprint)
        {
            m_devices.removeAt(index);
            return true;
        }
    }

    return false;
}

bool RosterPerson::isValid() const
{
    if (!m_member.isValid() || m_displayName.isEmpty() || m_devices.isEmpty())
    {
        return false;
    }

    for (const RosterDevice& device : m_devices)
    {
        if (!device.isValid())
        {
            return false;
        }
    }

    return true;
}

QJsonObject RosterPerson::toJson() const
{
    QJsonObject json;
    json[KeyMember] = m_member.toHex();
    json[KeyDisplayName] = m_displayName;
    json[KeyRole] = m_role == Role::Admin ? AdminText : MemberText;

    QJsonArray deviceArray;
    for (const RosterDevice& device : devices())
    {
        deviceArray.append(device.toJson());
    }
    json[KeyDevices] = deviceArray;

    return json;
}

std::optional<RosterPerson> RosterPerson::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json, {KeyMember, KeyDisplayName, KeyRole, KeyDevices}))
    {
        return std::nullopt;
    }

    const QJsonValue member = json.value(KeyMember);
    const QJsonValue displayName = json.value(KeyDisplayName);
    const QJsonValue role = json.value(KeyRole);
    const QJsonValue devices = json.value(KeyDevices);

    if (!member.isString() || !displayName.isString() || !role.isString() || !devices.isArray())
    {
        return std::nullopt;
    }

    // Exactly one of the two spellings: a role this build does not know is not a
    // role to guess at.
    RosterPerson person;
    if (role.toString() == AdminText)
    {
        person.m_role = Role::Admin;
    }
    else if (role.toString() == MemberText)
    {
        person.m_role = Role::Member;
    }
    else
    {
        return std::nullopt;
    }

    person.m_member = MemberId::fromHex(member.toString());
    person.m_displayName = displayName.toString();

    for (const QJsonValue& element : devices.toArray())
    {
        if (!element.isObject())
        {
            return std::nullopt;
        }

        const std::optional<RosterDevice> device = RosterDevice::fromJson(element.toObject());
        if (!device.has_value())
        {
            return std::nullopt;
        }

        // A device listed twice under one person is a malformed roster, not
        // something to quietly collapse.
        if (person.hasDevice(device.value().fingerprint()))
        {
            return std::nullopt;
        }

        person.m_devices.append(device.value());
    }

    if (!person.isValid())
    {
        return std::nullopt;
    }

    return person;
}

bool RosterPerson::operator==(const RosterPerson& other) const
{
    return m_member == other.m_member
        && m_displayName == other.m_displayName
        && m_role == other.m_role
        && devices() == other.devices();
}
