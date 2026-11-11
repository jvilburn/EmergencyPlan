#include "AccessList.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "Identity.h"

#include <QJsonArray>
#include <QObject>

QString accessRoleName(AccessRole role)
{
    switch (role)
    {
    case AccessRole::Admin:
        return "admin";
    case AccessRole::Member:
        return "member";
    }
    return QString();
}

AccessRole readAccessRole(JsonReader& json, const QString& key)
{
    return json.word(key, QList<AccessRole>{AccessRole::Admin, AccessRole::Member}, &accessRoleName);
}

// ============================================================================
// AccessDevice
// ============================================================================

AccessDevice::AccessDevice(const QString& name, const QByteArray& publicKey, const QByteArray& verifyKey)
    : m_name(name)
    , m_publicKey(publicKey)
    , m_verifyKey(verifyKey)
{
}

QJsonObject AccessDevice::toJson() const
{
    QJsonObject json;
    json["name"] = m_name;
    json["publicKey"] = Base64Url::encode(m_publicKey);
    json["verifyKey"] = Base64Url::encode(m_verifyKey);
    return json;
}

AccessDevice AccessDevice::fromJson(JsonReader json)
{
    QString name = json.string("name");
    QByteArray publicKey = json.bytes("publicKey", Crypto::KeyBytes);
    QByteArray verifyKey = json.bytes("verifyKey", Crypto::KeyBytes);
    json.finish();
    return AccessDevice(name, publicKey, verifyKey);
}

QByteArray AccessDevice::fingerprint() const
{
    return Identity::fingerprintOf(m_publicKey, m_verifyKey);
}

bool AccessDevice::operator==(const AccessDevice& other) const
{
    return m_name == other.m_name
        && m_publicKey == other.m_publicKey
        && m_verifyKey == other.m_verifyKey;
}

// ============================================================================
// AccessUser
// ============================================================================

AccessUser::AccessUser(const UserId& id, const QString& name, AccessRole role, const QList<AccessDevice>& devices)
    : m_id(id)
    , m_name(name)
    , m_role(role)
    , m_devices(devices)
{
}

QJsonObject AccessUser::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["name"] = m_name;
    json["role"] = accessRoleName(m_role);
    QJsonArray devices;
    for (const AccessDevice& device : m_devices)
    {
        devices.append(device.toJson());
    }
    json["devices"] = devices;
    return json;
}

AccessUser AccessUser::fromJson(JsonReader json)
{
    UserId id = UserId::fromBytes(json.bytes("id", Crypto::IdBytes));
    QString name = json.string("name");
    AccessRole role = readAccessRole(json, "role");
    QList<AccessDevice> devices;
    const QList<JsonReader> deviceReaders = json.objects("devices");
    for (const JsonReader& device : deviceReaders)
    {
        devices.append(AccessDevice::fromJson(device));
    }
    json.finish();
    return AccessUser(id, name, role, devices);
}

bool AccessUser::operator==(const AccessUser& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_role == other.m_role
        && m_devices == other.m_devices;
}

// ============================================================================
// AccessInvite
// ============================================================================

AccessInvite::AccessInvite(const UserId& maker, const std::optional<InvitedUser>& invitedUser,
                           const QByteArray& publicKey, const QByteArray& verifyKey)
    : m_maker(maker)
    , m_invitedUser(invitedUser)
    , m_publicKey(publicKey)
    , m_verifyKey(verifyKey)
{
}

AccessInvite AccessInvite::forDevice(const UserId& maker, const QByteArray& publicKey, const QByteArray& verifyKey)
{
    return AccessInvite(maker, std::nullopt, publicKey, verifyKey);
}

AccessInvite AccessInvite::forUser(const UserId& maker, AccessRole role, const QString& label,
                                   const QByteArray& publicKey, const QByteArray& verifyKey)
{
    return AccessInvite(maker, InvitedUser{role, label}, publicKey, verifyKey);
}

QJsonObject AccessInvite::toJson() const
{
    QJsonObject json;
    json["maker"] = m_maker.toString();
    json["publicKey"] = Base64Url::encode(m_publicKey);
    json["verifyKey"] = Base64Url::encode(m_verifyKey);
    if (m_invitedUser.has_value())
    {
        json["for"] = QString("user");
        json["role"] = accessRoleName(m_invitedUser->role);
        json["label"] = m_invitedUser->label;
    }
    else
    {
        json["for"] = QString("device");
    }
    return json;
}

AccessInvite AccessInvite::fromJson(JsonReader json)
{
    QString kind = json.word("for", {"device", "user"});
    UserId maker = UserId::fromBytes(json.bytes("maker", Crypto::IdBytes));
    QByteArray publicKey = json.bytes("publicKey", Crypto::KeyBytes);
    QByteArray verifyKey = json.bytes("verifyKey", Crypto::KeyBytes);
    if (kind == "user")
    {
        AccessRole role = readAccessRole(json, "role");
        QString label = json.string("label");
        json.finish();
        return forUser(maker, role, label, publicKey, verifyKey);
    }
    json.finish();
    return forDevice(maker, publicKey, verifyKey);
}

QByteArray AccessInvite::fingerprint() const
{
    return Identity::fingerprintOf(m_publicKey, m_verifyKey);
}

bool AccessInvite::operator==(const AccessInvite& other) const
{
    return m_maker == other.m_maker
        && m_invitedUser == other.m_invitedUser
        && m_publicKey == other.m_publicKey
        && m_verifyKey == other.m_verifyKey;
}

// ============================================================================
// AccessList
// ============================================================================

AccessList::AccessList(const QList<AccessUser>& users, const QList<AccessInvite>& invites, const QList<QByteArray>& formerKeys)
    : m_users(users)
    , m_invites(invites)
    , m_formerKeys(formerKeys)
{
}

AccessList AccessList::founding(const UserId& user, const QString& userName, const QString& deviceName,
                                const Identity& device)
{
    AccessDevice entry(deviceName, device.publicKey(), device.verifyKey());
    return AccessList({AccessUser(user, userName, AccessRole::Admin, {entry})}, {}, {});
}

QByteArray AccessList::formerKeyHash(const QByteArray& dataKey)
{
    return Crypto::hash(dataKey, Crypto::HashBytes, "wp-former-key");
}

QList<RecipientKeys> AccessList::recipients() const
{
    QList<RecipientKeys> recipients;
    for (const AccessUser& user : m_users)
    {
        for (const AccessDevice& device : user.devices())
        {
            recipients.append(RecipientKeys{device.publicKey(), device.verifyKey()});
        }
    }
    for (const AccessInvite& invite : m_invites)
    {
        recipients.append(RecipientKeys{invite.publicKey(), invite.verifyKey()});
    }
    return recipients;
}

std::optional<QByteArray> AccessList::verifyKeyOf(const QByteArray& fingerprint) const
{
    const QList<RecipientKeys> keys = recipients();
    for (const RecipientKeys& recipient : keys)
    {
        if (Identity::fingerprintOf(recipient.publicKey, recipient.verifyKey) == fingerprint)
        {
            return recipient.verifyKey;
        }
    }
    return std::nullopt;
}

QSet<QByteArray> AccessList::recipientFingerprints() const
{
    QSet<QByteArray> recipients = deviceFingerprints();
    for (const AccessInvite& invite : m_invites)
    {
        recipients.insert(invite.fingerprint());
    }
    return recipients;
}

QSet<QByteArray> AccessList::deviceFingerprints() const
{
    QSet<QByteArray> devices;
    for (const AccessUser& user : m_users)
    {
        for (const AccessDevice& device : user.devices())
        {
            devices.insert(device.fingerprint());
        }
    }
    return devices;
}

std::optional<AccessUser> AccessList::findUser(const UserId& id) const
{
    for (const AccessUser& user : m_users)
    {
        if (user.id() == id)
        {
            return user;
        }
    }
    return std::nullopt;
}

std::optional<AccessUser> AccessList::userOfDevice(const QByteArray& device) const
{
    for (const AccessUser& user : m_users)
    {
        for (const AccessDevice& entry : user.devices())
        {
            if (entry.fingerprint() == device)
            {
                return user;
            }
        }
    }
    return std::nullopt;
}

std::optional<AccessDevice> AccessList::findDevice(const QByteArray& device) const
{
    for (const AccessUser& user : m_users)
    {
        for (const AccessDevice& entry : user.devices())
        {
            if (entry.fingerprint() == device)
            {
                return entry;
            }
        }
    }
    return std::nullopt;
}

std::optional<AccessInvite> AccessList::findInvite(const QByteArray& invite) const
{
    for (const AccessInvite& entry : m_invites)
    {
        if (entry.fingerprint() == invite)
        {
            return entry;
        }
    }
    return std::nullopt;
}

QStringList AccessList::adminNames() const
{
    QStringList names;
    for (const AccessUser& user : m_users)
    {
        if (user.role() == AccessRole::Admin)
        {
            names.append(user.name());
        }
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

QString AccessList::describeInvite(const AccessInvite& invite) const
{
    if (invite.invitedUser().has_value())
    {
        return QObject::tr("for %1").arg(invite.invitedUser()->label);
    }
    return QObject::tr("a device for %1").arg(findUser(invite.maker())->name());
}

void AccessList::setUser(const AccessUser& user)
{
    for (AccessUser& existing : m_users)
    {
        if (existing.id() == user.id())
        {
            existing = user;
            return;
        }
    }
    m_users.append(user);
}

void AccessList::removeUser(const UserId& id)
{
    m_users.removeIf([&id](const AccessUser& user) { return user.id() == id; });
    m_invites.removeIf([&id](const AccessInvite& invite) { return invite.maker() == id; });
}

void AccessList::removeDevice(const QByteArray& device)
{
    for (AccessUser& user : m_users)
    {
        QList<AccessDevice> devices = user.devices();
        devices.removeIf([&device](const AccessDevice& entry) { return entry.fingerprint() == device; });
        user.setDevices(devices);
    }
}

void AccessList::removeInvite(const QByteArray& invite)
{
    m_invites.removeIf([&invite](const AccessInvite& entry) { return entry.fingerprint() == invite; });
}

QJsonObject AccessList::toJson() const
{
    QJsonObject json;
    QJsonArray users;
    for (const AccessUser& user : m_users)
    {
        users.append(user.toJson());
    }
    json["users"] = users;
    if (!m_invites.isEmpty())
    {
        QJsonArray invites;
        for (const AccessInvite& invite : m_invites)
        {
            invites.append(invite.toJson());
        }
        json["invites"] = invites;
    }
    if (!m_formerKeys.isEmpty())
    {
        QJsonArray formerKeys;
        for (const QByteArray& formerKey : m_formerKeys)
        {
            formerKeys.append(Base64Url::encode(formerKey));
        }
        json["formerKeys"] = formerKeys;
    }
    return json;
}

AccessList AccessList::fromJson(JsonReader json)
{
    QList<AccessUser> users;
    const QList<JsonReader> userReaders = json.objects("users");
    for (const JsonReader& user : userReaders)
    {
        users.append(AccessUser::fromJson(user));
    }
    QList<AccessInvite> invites;
    const QList<JsonReader> inviteReaders = json.optionalObjects("invites");
    for (const JsonReader& invite : inviteReaders)
    {
        invites.append(AccessInvite::fromJson(invite));
    }
    QList<QByteArray> formerKeys = json.optionalBytesList("formerKeys", Crypto::KeyBytes);
    json.finish();
    return AccessList(users, invites, formerKeys);
}

bool AccessList::operator==(const AccessList& other) const
{
    return m_users == other.m_users
        && m_invites == other.m_invites
        && m_formerKeys == other.m_formerKeys;
}
