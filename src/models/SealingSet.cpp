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
const QLatin1String KeyMaker("maker");
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
    return purpose == other.purpose && maker == other.maker && role == other.role && label == other.label
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

std::optional<SealingSet::DeviceAt> SealingSet::find(const Fingerprint& device) const
{
    for (auto person = m_people.constBegin(); person != m_people.constEnd(); ++person)
    {
        for (qsizetype i = 0; i < person.value().devices.size(); ++i)
        {
            if (person.value().devices.at(i).fingerprint() == device)
            {
                return DeviceAt{person.key(), i};
            }
        }
    }
    return std::nullopt;
}

std::optional<Holder> SealingSet::personOf(const Fingerprint& device) const
{
    const std::optional<DeviceAt> at = find(device);
    return at.has_value() ? std::optional<Holder>(m_people.value(at->person)) : std::nullopt;
}

std::optional<NamedDevice> SealingSet::device(const Fingerprint& fingerprint) const
{
    const std::optional<DeviceAt> at = find(fingerprint);
    return at.has_value() ? std::optional<NamedDevice>(m_people.value(at->person).devices.at(at->index)) : std::nullopt;
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

bool SealingSet::keysAreFree(const DeviceKeys& keys) const
{
    for (const DeviceKeys& held : recipients())
    {
        if (held.publicKey == keys.publicKey || held.verifyKey == keys.verifyKey)
        {
            return false;
        }
    }
    return true;
}

bool SealingSet::addInvite(const Invite& invite)
{
    if (!invite.keys.isValid() || !invite.maker.isValid() || !m_people.contains(invite.maker.toHex())
        || !keysAreFree(invite.keys))
    {
        return false;
    }
    m_invites.insert(fingerprintHex(invite.keys), invite);
    return true;
}

bool SealingSet::removeInvite(const Fingerprint& invite)
{
    return m_invites.remove(invite.toHex());
}

bool SealingSet::join(const Fingerprint& invite, const NamedDevice& device, const MemberId& joiner, const QString& name)
{
    const auto found = m_invites.constFind(invite.toHex());
    if (found == m_invites.constEnd() || !device.keys.isValid() || !keysAreFree(device.keys))
    {
        return false;
    }
    const Invite entry = found.value();

    QString personId;
    switch (entry.purpose)
    {
    case Invite::For::Device:
        personId = entry.maker.toHex();
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
        m_people.insert(personId, Holder{joiner, name.trimmed(), entry.role, {}});
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
    if (m_people.size() == 1 || !m_people.remove(id.toHex()))
    {
        return false;
    }
    dropInvitesWithoutAPerson();
    return true;
}

void SealingSet::dropInvitesWithoutAPerson()
{
    for (auto invite = m_invites.begin(); invite != m_invites.end();)
    {
        if (m_people.contains(invite.value().maker.toHex()))
        {
            ++invite;
        }
        else
        {
            invite = m_invites.erase(invite);
        }
    }
}

bool SealingSet::removeDevice(const Fingerprint& device)
{
    const std::optional<DeviceAt> at = find(device);
    if (!at.has_value() || m_people[at->person].devices.size() == 1)
    {
        return false;
    }
    m_people[at->person].devices.removeAt(at->index);
    return true;
}

bool SealingSet::setRole(const MemberId& id, Role role)
{
    const auto found = m_people.find(id.toHex());
    if (found == m_people.end() || found.value().role == role)
    {
        return false;
    }
    found.value().role = role;
    return true;
}

// --- Merging ---------------------------------------------------------------------

namespace
{

/// One entry's three sides, by its key in each map, into merged: mine when theirs is
/// as base, else theirs. absent is a value too.
template<typename T>
void mergeEntry(const QHash<QString, T>& base,
                const QHash<QString, T>& mine,
                const QHash<QString, T>& theirs,
                const QString& key,
                QHash<QString, T>& merged)
{
    const std::optional<T> b = base.contains(key) ? std::optional<T>(base.value(key)) : std::nullopt;
    const std::optional<T> m = mine.contains(key) ? std::optional<T>(mine.value(key)) : std::nullopt;
    const std::optional<T> t = theirs.contains(key) ? std::optional<T>(theirs.value(key)) : std::nullopt;

    const std::optional<T> result = t == b ? m : t;
    if (result.has_value())
    {
        merged.insert(key, result.value());
    }
    else
    {
        merged.remove(key);
    }
}

template<typename T>
QSet<QString> allKeys(const QHash<QString, T>& a, const QHash<QString, T>& b, const QHash<QString, T>& c)
{
    QSet<QString> keys;
    for (const QHash<QString, T>* map : {&a, &b, &c})
    {
        for (auto it = map->constBegin(); it != map->constEnd(); ++it)
        {
            keys.insert(it.key());
        }
    }
    return keys;
}

} // namespace

SealingSet SealingSet::merge(const SealingSet& base, const SealingSet& mine, const SealingSet& theirs)
{
    SealingSet merged = theirs;
    for (const QString& id : allKeys(base.m_people, mine.m_people, theirs.m_people))
    {
        mergeEntry(base.m_people, mine.m_people, theirs.m_people, id, merged.m_people);
    }
    for (const QString& fingerprint : allKeys(base.m_invites, mine.m_invites, theirs.m_invites))
    {
        mergeEntry(base.m_invites, mine.m_invites, theirs.m_invites, fingerprint, merged.m_invites);
    }
    for (const QString& hash : mine.m_formerKeys)
    {
        if (!merged.m_formerKeys.contains(hash))
        {
            merged.m_formerKeys.append(hash);
        }
    }

    merged.dropInvitesWithoutAPerson();

    // Two removals that between them leave nobody: theirs, which the server holds,
    // stands.
    return merged.m_people.isEmpty() ? theirs : merged;
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
    const QString hash = formerKeyHash(key);
    if (!m_formerKeys.contains(hash))
    {
        m_formerKeys.append(hash);
    }
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
        inviteJson[KeyMaker] = invite.maker.toHex();
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

        // In the set as each device is read, so the keys of one device count against the next.
        Holder& added = set.m_people[id];
        added = person;
        for (const QJsonObject& deviceJson : devices)
        {
            NamedDevice device;
            if (!StrictJson::hasOnlyKeys(deviceJson, {KeyName, KeyPublicKey, KeyVerifyKey})
                || !StrictJson::readString(deviceJson, KeyName, device.name) || !readKeys(deviceJson, device.keys)
                || !set.keysAreFree(device.keys))
            {
                return std::nullopt;
            }
            added.devices.append(device);
        }
        std::sort(added.devices.begin(), added.devices.end(), &byFingerprint);
    }

    for (const QJsonObject& inviteJson : invites)
    {
        Invite invite;
        QString purpose;
        QString maker;
        QString role;
        if (!StrictJson::hasOnlyKeys(inviteJson, {KeyFor, KeyMaker, KeyRole, KeyLabel, KeyPublicKey, KeyVerifyKey})
            || !StrictJson::readString(inviteJson, KeyFor, purpose) || !StrictJson::readString(inviteJson, KeyMaker, maker)
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
        invite.maker = MemberId::fromHex(maker);
        invite.role = roleOf(role).value();
        if (maker != invite.maker.toHex() || !set.addInvite(invite))
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
