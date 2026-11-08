#include "AccessList.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "Identity.h"

#include <QJsonArray>

namespace
{

QString roleName(AccessRole role)
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

AccessRole readRole(JsonReader& json, const QString& key)
{
    QString role = json.string(key);
    if (role == "member")
    {
        return AccessRole::Member;
    }
    if (role != "admin")
    {
        json.fail(key, "is not admin or member");
    }
    return AccessRole::Admin;
}

UserId readUserId(JsonReader& json, const QString& key)
{
    return UserId::fromString(Base64Url::encode(json.bytes(key, Crypto::IdBytes)));
}

}  // namespace

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
    json["role"] = roleName(m_role);
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
    UserId id = readUserId(json, "id");
    QString name = json.string("name");
    AccessRole role = readRole(json, "role");
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

AccessInvite::AccessInvite(InviteKind kind, const UserId& maker, std::optional<AccessRole> role, const QString& label,
                           const QByteArray& publicKey, const QByteArray& verifyKey)
    : m_kind(kind)
    , m_maker(maker)
    , m_role(role)
    , m_label(label)
    , m_publicKey(publicKey)
    , m_verifyKey(verifyKey)
{
}

AccessInvite AccessInvite::forDevice(const UserId& maker, const QByteArray& publicKey, const QByteArray& verifyKey)
{
    return AccessInvite(InviteKind::Device, maker, std::nullopt, QString(), publicKey, verifyKey);
}

AccessInvite AccessInvite::forUser(const UserId& maker, AccessRole role, const QString& label,
                                   const QByteArray& publicKey, const QByteArray& verifyKey)
{
    return AccessInvite(InviteKind::User, maker, role, label, publicKey, verifyKey);
}

QJsonObject AccessInvite::toJson() const
{
    QJsonObject json;
    json["maker"] = m_maker.toString();
    json["publicKey"] = Base64Url::encode(m_publicKey);
    json["verifyKey"] = Base64Url::encode(m_verifyKey);
    switch (m_kind)
    {
    case InviteKind::Device:
        json["for"] = QString("device");
        break;
    case InviteKind::User:
        json["for"] = QString("user");
        json["role"] = roleName(m_role.value());
        json["label"] = m_label;
        break;
    }
    return json;
}

AccessInvite AccessInvite::fromJson(JsonReader json)
{
    QString kind = json.string("for");
    UserId maker = readUserId(json, "maker");
    QByteArray publicKey = json.bytes("publicKey", Crypto::KeyBytes);
    QByteArray verifyKey = json.bytes("verifyKey", Crypto::KeyBytes);
    if (kind == "user")
    {
        AccessRole role = readRole(json, "role");
        QString label = json.string("label");
        json.finish();
        return forUser(maker, role, label, publicKey, verifyKey);
    }
    if (kind != "device")
    {
        json.fail("for", "is not device or user");
    }
    json.finish();
    return forDevice(maker, publicKey, verifyKey);
}

bool AccessInvite::operator==(const AccessInvite& other) const
{
    return m_kind == other.m_kind
        && m_maker == other.m_maker
        && m_role == other.m_role
        && m_label == other.m_label
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
