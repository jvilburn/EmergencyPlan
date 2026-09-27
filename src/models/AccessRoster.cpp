#include "AccessRoster.h"

#include <QJsonArray>
#include <QStringList>

#include <algorithm>

namespace
{

/// True when another device — a different fingerprint — already uses either of
/// this device's keys.
bool sharesAKey(const RosterDevice& device, const QList<RosterDevice>& others)
{
    for (const RosterDevice& other : others)
    {
        if (other.fingerprint() == device.fingerprint())
        {
            continue;
        }

        if (other.publicKey() == device.publicKey() || other.verifyKey() == device.verifyKey())
        {
            return true;
        }
    }

    return false;
}

} // namespace

bool AccessRoster::addPerson(const RosterPerson& person)
{
    if (!person.isValid() || m_people.contains(person.member().toHex()))
    {
        return false;
    }

    // Every device of the new person must be unclaimed, and must not share a key
    // with any device already here or with another of the person's own.
    QList<RosterDevice> claimed = allDevices();
    for (const RosterDevice& device : person.devices())
    {
        if (personFor(device.fingerprint()).has_value() || sharesAKey(device, claimed))
        {
            return false;
        }

        claimed.append(device);
    }

    m_people.insert(person.member().toHex(), person);
    return true;
}

bool AccessRoster::removePerson(const MemberId& member)
{
    return m_people.remove(member.toHex()) > 0;
}

bool AccessRoster::contains(const MemberId& member) const
{
    return m_people.contains(member.toHex());
}

bool AccessRoster::addDevice(const MemberId& member, const RosterDevice& device)
{
    if (!device.isValid())
    {
        return false;
    }

    const auto entry = m_people.find(member.toHex());
    if (entry == m_people.end())
    {
        return false;
    }

    const std::optional<RosterPerson> owner = personFor(device.fingerprint());
    if (owner.has_value() && !(owner.value().member() == member))
    {
        return false;
    }

    if (sharesAKey(device, allDevices()))
    {
        return false;
    }

    return entry.value().addDevice(device);
}

bool AccessRoster::removeDevice(const MemberId& member, const Fingerprint& fingerprint)
{
    const auto entry = m_people.find(member.toHex());
    if (entry == m_people.end())
    {
        return false;
    }

    return entry.value().removeDevice(fingerprint);
}

bool AccessRoster::setRole(const MemberId& member, RosterPerson::Role role)
{
    const auto entry = m_people.find(member.toHex());
    if (entry == m_people.end())
    {
        return false;
    }

    entry.value().setRole(role);
    return true;
}

QList<MemberId> AccessRoster::admins() const
{
    // Keyed by member id hex in a hash, so the order is imposed here.
    QStringList keys;
    for (auto entry = m_people.constBegin(); entry != m_people.constEnd(); ++entry)
    {
        if (entry.value().isAdmin())
        {
            keys.append(entry.key());
        }
    }
    keys.sort();

    QList<MemberId> admins;
    for (const QString& key : keys)
    {
        admins.append(MemberId::fromHex(key));
    }
    return admins;
}

std::optional<RosterPerson> AccessRoster::person(const MemberId& member) const
{
    const auto entry = m_people.constFind(member.toHex());
    if (entry == m_people.constEnd())
    {
        return std::nullopt;
    }

    return entry.value();
}

std::optional<RosterPerson> AccessRoster::personFor(const Fingerprint& fingerprint) const
{
    if (!fingerprint.isValid())
    {
        return std::nullopt;
    }

    for (auto entry = m_people.constBegin(); entry != m_people.constEnd(); ++entry)
    {
        if (entry.value().hasDevice(fingerprint))
        {
            return entry.value();
        }
    }

    return std::nullopt;
}

std::optional<RosterDevice> AccessRoster::deviceFor(const Fingerprint& fingerprint) const
{
    const std::optional<RosterPerson> owner = personFor(fingerprint);
    if (!owner.has_value())
    {
        return std::nullopt;
    }

    return owner.value().deviceFor(fingerprint);
}

QList<RosterPerson> AccessRoster::people() const
{
    QList<RosterPerson> sorted = m_people.values();
    std::sort(sorted.begin(),
              sorted.end(),
              [](const RosterPerson& left, const RosterPerson& right)
              {
                  // Case-insensitive first, so "de Groot" sorts among the Ds.
                  // Not locale-aware, so the order is the same on every device.
                  const int byName = QString::compare(left.displayName(),
                                                      right.displayName(),
                                                      Qt::CaseInsensitive);
                  if (byName != 0)
                  {
                      return byName < 0;
                  }
                  if (left.displayName() != right.displayName())
                  {
                      return left.displayName() < right.displayName();
                  }
                  return left.member().toHex() < right.member().toHex();
              });
    return sorted;
}

QList<RosterDevice> AccessRoster::allDevices() const
{
    QList<RosterDevice> devices;
    for (auto entry = m_people.constBegin(); entry != m_people.constEnd(); ++entry)
    {
        devices.append(entry.value().devices());
    }
    return devices;
}

QJsonObject AccessRoster::toJson() const
{
    QJsonArray peopleArray;
    for (const RosterPerson& person : people())
    {
        peopleArray.append(person.toJson());
    }

    QJsonObject json;
    json["people"] = peopleArray;
    return json;
}

std::optional<AccessRoster> AccessRoster::fromJson(const QJsonObject& json)
{
    const QJsonValue people = json.value("people");
    if (!people.isArray())
    {
        return std::nullopt;
    }

    AccessRoster roster;
    for (const QJsonValue& element : people.toArray())
    {
        if (!element.isObject())
        {
            return std::nullopt;
        }

        const std::optional<RosterPerson> person = RosterPerson::fromJson(element.toObject());
        if (!person.has_value())
        {
            return std::nullopt;
        }

        // addPerson refuses a repeated member id and a device already claimed
        // elsewhere, so both are refusals of the whole roster rather than
        // something silently dropped.
        if (!roster.addPerson(person.value()))
        {
            return std::nullopt;
        }
    }

    return roster;
}

bool AccessRoster::operator==(const AccessRoster& other) const
{
    return m_people == other.m_people;
}
