#include "SealingSet.h"

#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "StrictJson.h"

#include <QJsonArray>

#include <algorithm>

namespace
{

// Part of the payload format.
const QLatin1String KeyPeople("people");
const QLatin1String KeyInvites("invites");
const QLatin1String KeyFormerKeys("formerKeys");
const QLatin1String KeyId("id");
const QLatin1String KeyName("name");
const QLatin1String KeyRole("role");
const QLatin1String KeyDevices("devices");
const QLatin1String KeyFor("for");
const QLatin1String KeyPerson("person");
const QLatin1String KeyLabel("label");
const QLatin1String KeyPublicKey("publicKey");
const QLatin1String KeyVerifyKey("verifyKey");

const QLatin1String RoleAdmin("admin");
const QLatin1String RoleMember("member");
const QLatin1String ForDevice("device");
const QLatin1String ForUser("user");

constexpr int FormerKeyHashSize = 32;

QString formerKeyHash(const DataKey& key)
{
    return HexCodec::encode(DocumentCrypto::hash(HashDomain::FormerKey, key.bytes, FormerKeyHashSize));
}

QString fingerprintHex(const DeviceKeys& keys)
{
    return DocumentCrypto::fingerprintOf(keys.publicKey, keys.verifyKey).toHex();
}

QString roleName(Role role)
{
    return role == Role::Admin ? RoleAdmin : RoleMember;
}

std::optional<Role> roleOf(const QString& name)
{
    if (name == RoleAdmin)
    {
        return Role::Admin;
    }
    if (name == RoleMember)
    {
        return Role::Member;
    }
    return std::nullopt;
}

bool readKeys(const QJsonObject& json, DeviceKeys& out)
{
    QString publicHex;
    QString verifyHex;
    if (!StrictJson::readString(json, KeyPublicKey, publicHex) || !StrictJson::readString(json, KeyVerifyKey, verifyHex))
    {
        return false;
    }
    out.publicKey.bytes = HexCodec::decode(publicHex, PublicKey::Size);
    out.verifyKey.bytes = HexCodec::decode(verifyHex, VerifyKey::Size);
    return out.isValid();
}

void writeKeys(QJsonObject& json, const DeviceKeys& keys)
{
    json[KeyPublicKey] = HexCodec::encode(keys.publicKey.bytes);
    json[KeyVerifyKey] = HexCodec::encode(keys.verifyKey.bytes);
}

bool byFingerprint(const NamedDevice& a, const NamedDevice& b)
{
    return a.fingerprint().toHex() < b.fingerprint().toHex();
}

} // namespace

// --- The entries -----------------------------------------------------------------

Fingerprint NamedDevice::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(keys.publicKey, keys.verifyKey);
}

bool Holder::operator==(const Holder& other) const
{
    return id == other.id && name == other.name && role == other.role && devices == other.devices;
}

Fingerprint Invite::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(keys.publicKey, keys.verifyKey);
}

bool Invite::operator==(const Invite& other) const
{
    return purpose == other.purpose && person == other.person && role == other.role && label == other.label
        && keys == other.keys;
}

// --- The set ---------------------------------------------------------------------

SealingSet SealingSet::of(const MemberId& person, const QString& name, const NamedDevice& device)
{
    SealingSet set;
    if (!person.isValid() || !device.keys.isValid())
    {
        return set;
    }
    set.m_people.insert(person.toHex(), Holder{person, name, Role::Admin, {device}});
    return set;
}

QList<Holder> SealingSet::people() const
{
    QStringList ids = m_people.keys();
    std::sort(ids.begin(), ids.end());

    QList<Holder> people;
    for (const QString& id : ids)
    {
        people.append(m_people.value(id));
    }
    return people;
}

std::optional<Holder> SealingSet::person(const MemberId& id) const
{
    const auto found = m_people.constFind(id.toHex());
    if (found == m_people.constEnd())
    {
        return std::nullopt;
    }
    return found.value();
}

std::optional<Holder> SealingSet::personOf(const Fingerprint& device) const
{
    for (const Holder& person : m_people)
    {
        for (const NamedDevice& candidate : person.devices)
        {
            if (candidate.fingerprint() == device)
            {
                return person;
            }
        }
    }
    return std::nullopt;
}

std::optional<NamedDevice> SealingSet::device(const Fingerprint& fingerprint) const
{
    for (const Holder& person : m_people)
    {
        for (const NamedDevice& candidate : person.devices)
        {
            if (candidate.fingerprint() == fingerprint)
            {
                return candidate;
            }
        }
    }
    return std::nullopt;
}

QList<Invite> SealingSet::invites() const
{
    QStringList fingerprints = m_invites.keys();
    std::sort(fingerprints.begin(), fingerprints.end());

    QList<Invite> invites;
    for (const QString& fingerprint : fingerprints)
    {
        invites.append(m_invites.value(fingerprint));
    }
    return invites;
}

std::optional<Invite> SealingSet::invite(const Fingerprint& fingerprint) const
{
    const auto found = m_invites.constFind(fingerprint.toHex());
    if (found == m_invites.constEnd())
    {
        return std::nullopt;
    }
    return found.value();
}

bool SealingSet::contains(const Fingerprint& fingerprint) const
{
    return m_invites.contains(fingerprint.toHex()) || device(fingerprint).has_value();
}

int SealingSet::adminCount() const
{
    int admins = 0;
    for (const Holder& person : m_people)
    {
        admins += person.isAdmin() ? 1 : 0;
    }
    return admins;
}

QList<DeviceKeys> SealingSet::recipients() const
{
    QHash<QString, DeviceKeys> byFingerprint;
    for (const Holder& person : m_people)
    {
        for (const NamedDevice& candidate : person.devices)
        {
            byFingerprint.insert(fingerprintHex(candidate.keys), candidate.keys);
        }
    }
    for (auto it = m_invites.constBegin(); it != m_invites.constEnd(); ++it)
    {
        byFingerprint.insert(it.key(), it.value().keys);
    }

    QStringList fingerprints = byFingerprint.keys();
    std::sort(fingerprints.begin(), fingerprints.end());
    QList<DeviceKeys> recipients;
    for (const QString& fingerprint : fingerprints)
    {
        recipients.append(byFingerprint.value(fingerprint));
    }
    return recipients;
}

// --- Writing ---------------------------------------------------------------------

bool SealingSet::keysAreFree(const DeviceKeys& keys, const QString& except) const
{
    for (const DeviceKeys& held : recipients())
    {
        if (fingerprintHex(held) == except)
        {
            continue;
        }
        if (held.publicKey == keys.publicKey || held.verifyKey == keys.verifyKey)
        {
            return false;
        }
    }
    return true;
}

bool SealingSet::addInvite(const Invite& invite)
{
    if (!invite.keys.isValid() || !invite.person.isValid() || !m_people.contains(invite.person.toHex())
        || !keysAreFree(invite.keys, QString()))
    {
        return false;
    }
    m_invites.insert(fingerprintHex(invite.keys), invite);
    return true;
}

bool SealingSet::removeInvite(const Fingerprint& invite)
{
    return m_invites.remove(invite.toHex()) > 0;
}

bool SealingSet::join(const Fingerprint& invite, const NamedDevice& device, const MemberId& joiner, const QString& name)
{
    const auto found = m_invites.constFind(invite.toHex());
    if (found == m_invites.constEnd() || !device.keys.isValid() || !keysAreFree(device.keys, invite.toHex()))
    {
        return false;
    }
    const Invite open = found.value();

    QString personId;
    switch (open.purpose)
    {
    case Invite::For::Device:
        personId = open.person.toHex();
        if (!m_people.contains(personId))
        {
            return false;
        }
        break;
    case Invite::For::User:
        personId = joiner.toHex();
        if (!joiner.isValid() || name.trimmed().isEmpty() || m_people.contains(personId))
        {
            return false;
        }
        m_people.insert(personId, Holder{joiner, name.trimmed(), open.role, {}});
        break;
    }

    Holder& person = m_people[personId];
    person.devices.append(device);
    std::sort(person.devices.begin(), person.devices.end(), &byFingerprint);
    m_invites.remove(invite.toHex());
    return true;
}

bool SealingSet::removePerson(const MemberId& id)
{
    if (m_people.size() == 1 || m_people.remove(id.toHex()) == 0)
    {
        return false;
    }

    // Invites that person made go with them; a Device invite into them would have
    // nowhere to join.
    const QStringList invites = m_invites.keys();
    for (const QString& fingerprint : invites)
    {
        if (m_invites.value(fingerprint).person == id)
        {
            m_invites.remove(fingerprint);
        }
    }
    return true;
}

bool SealingSet::removeDevice(const Fingerprint& device)
{
    for (Holder& person : m_people)
    {
        for (qsizetype i = 0; i < person.devices.size(); ++i)
        {
            if (person.devices.at(i).fingerprint() == device)
            {
                if (person.devices.size() == 1)
                {
                    return false;
                }
                person.devices.removeAt(i);
                return true;
            }
        }
    }
    return false;
}

bool SealingSet::setRole(const MemberId& id, Role role)
{
    const auto found = m_people.find(id.toHex());
    if (found == m_people.end())
    {
        return false;
    }
    found.value().role = role;
    return true;
}

// --- The rule --------------------------------------------------------------------

bool SealingSet::rotatesFrom(const QSet<QString>& previousRecipients) const
{
    QSet<QString> current;
    for (const DeviceKeys& keys : recipients())
    {
        current.insert(fingerprintHex(keys));
    }
    for (const QString& previous : previousRecipients)
    {
        if (!current.contains(previous))
        {
            return true;
        }
    }
    return false;
}

void SealingSet::addFormerKey(const DataKey& key)
{
    m_formerKeys.append(formerKeyHash(key));
}

bool SealingSet::descendsFrom(const DataKey& key) const
{
    return m_formerKeys.contains(formerKeyHash(key));
}

// --- JSON ------------------------------------------------------------------------

QJsonObject SealingSet::toJson() const
{
    QJsonArray people;
    for (const Holder& person : this->people())
    {
        QJsonArray devices;
        for (const NamedDevice& device : person.devices)
        {
            QJsonObject deviceJson;
            deviceJson[KeyName] = device.name;
            writeKeys(deviceJson, device.keys);
            devices.append(deviceJson);
        }
        QJsonObject personJson;
        personJson[KeyId] = person.id.toHex();
        personJson[KeyName] = person.name;
        personJson[KeyRole] = roleName(person.role);
        personJson[KeyDevices] = devices;
        people.append(personJson);
    }

    QJsonArray invites;
    for (const Invite& invite : this->invites())
    {
        QJsonObject inviteJson;
        inviteJson[KeyFor] = invite.purpose == Invite::For::Device ? ForDevice : ForUser;
        inviteJson[KeyPerson] = invite.person.toHex();
        inviteJson[KeyRole] = roleName(invite.role);
        inviteJson[KeyLabel] = invite.label;
        writeKeys(inviteJson, invite.keys);
        invites.append(inviteJson);
    }

    QJsonObject json;
    json[KeyPeople] = people;
    json[KeyInvites] = invites;
    json[KeyFormerKeys] = QJsonArray::fromStringList(m_formerKeys);
    return json;
}

std::optional<SealingSet> SealingSet::fromJson(const QJsonObject& json)
{
    SealingSet set;
    QList<QJsonObject> people;
    QList<QJsonObject> invites;
    if (!StrictJson::hasOnlyKeys(json, {KeyPeople, KeyInvites, KeyFormerKeys})
        || !StrictJson::readObjectArray(json, KeyPeople, people)
        || !StrictJson::readObjectArray(json, KeyInvites, invites)
        || !StrictJson::readStringArray(json, KeyFormerKeys, set.m_formerKeys))
    {
        return std::nullopt;
    }
    for (const QString& hash : set.m_formerKeys)
    {
        if (!HexCodec::isCanonical(hash, 2 * FormerKeyHashSize))
        {
            return std::nullopt;
        }
    }

    for (const QJsonObject& personJson : people)
    {
        Holder person;
        QString id;
        QString role;
        QList<QJsonObject> devices;
        if (!StrictJson::hasOnlyKeys(personJson, {KeyId, KeyName, KeyRole, KeyDevices})
            || !StrictJson::readString(personJson, KeyId, id) || !StrictJson::readString(personJson, KeyName, person.name)
            || !StrictJson::readString(personJson, KeyRole, role) || !roleOf(role).has_value()
            || !StrictJson::readObjectArray(personJson, KeyDevices, devices) || devices.isEmpty())
        {
            return std::nullopt;
        }
        person.id = MemberId::fromHex(id);
        person.role = roleOf(role).value();
        if (!person.id.isValid() || set.m_people.contains(id) || id != person.id.toHex())
        {
            return std::nullopt;
        }
        for (const QJsonObject& deviceJson : devices)
        {
            NamedDevice device;
            if (!StrictJson::hasOnlyKeys(deviceJson, {KeyName, KeyPublicKey, KeyVerifyKey})
                || !StrictJson::readString(deviceJson, KeyName, device.name) || !readKeys(deviceJson, device.keys)
                || !set.keysAreFree(device.keys, QString()))
            {
                return std::nullopt;
            }
            for (const NamedDevice& held : person.devices)
            {
                if (held.keys.publicKey == device.keys.publicKey || held.keys.verifyKey == device.keys.verifyKey)
                {
                    return std::nullopt;
                }
            }
            person.devices.append(device);
        }
        std::sort(person.devices.begin(), person.devices.end(), &byFingerprint);
        set.m_people.insert(id, person);
    }

    for (const QJsonObject& inviteJson : invites)
    {
        Invite invite;
        QString purpose;
        QString person;
        QString role;
        if (!StrictJson::hasOnlyKeys(inviteJson, {KeyFor, KeyPerson, KeyRole, KeyLabel, KeyPublicKey, KeyVerifyKey})
            || !StrictJson::readString(inviteJson, KeyFor, purpose) || !StrictJson::readString(inviteJson, KeyPerson, person)
            || !StrictJson::readString(inviteJson, KeyRole, role) || !roleOf(role).has_value()
            || !StrictJson::readString(inviteJson, KeyLabel, invite.label) || !readKeys(inviteJson, invite.keys))
        {
            return std::nullopt;
        }
        if (purpose == ForDevice)
        {
            invite.purpose = Invite::For::Device;
        }
        else if (purpose == ForUser)
        {
            invite.purpose = Invite::For::User;
        }
        else
        {
            return std::nullopt;
        }
        invite.person = MemberId::fromHex(person);
        invite.role = roleOf(role).value();
        if (!set.addInvite(invite))
        {
            return std::nullopt;
        }
    }

    if (set.m_people.isEmpty())
    {
        return std::nullopt;
    }
    return set;
}

bool SealingSet::operator==(const SealingSet& other) const
{
    return m_people == other.m_people && m_invites == other.m_invites && m_formerKeys == other.m_formerKeys;
}
