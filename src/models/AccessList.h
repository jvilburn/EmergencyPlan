#pragma once

#include "Id.h"
#include "JsonReader.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

class Identity;

enum class AccessRole
{
    Admin,
    Member
};

/// One of a user's devices, by the keys the data key is sealed to and its writes are
/// signed with.
class AccessDevice
{
public:
    AccessDevice(const QString& name, const QByteArray& publicKey, const QByteArray& verifyKey);

    const QString& name() const { return m_name; }
    const QByteArray& publicKey() const { return m_publicKey; }
    const QByteArray& verifyKey() const { return m_verifyKey; }

    QJsonObject toJson() const;
    static AccessDevice fromJson(JsonReader json);

    bool operator==(const AccessDevice& other) const;
    bool operator!=(const AccessDevice& other) const { return !(*this == other); }

private:
    QString m_name;
    QByteArray m_publicKey;
    QByteArray m_verifyKey;
};

/// A user of the ward: their id within it, the name they gave, their role, and their devices.
class AccessUser
{
public:
    AccessUser(const UserId& id, const QString& name, AccessRole role, const QList<AccessDevice>& devices);

    const UserId& id() const { return m_id; }
    const QString& name() const { return m_name; }
    AccessRole role() const { return m_role; }
    const QList<AccessDevice>& devices() const { return m_devices; }

    QJsonObject toJson() const;
    static AccessUser fromJson(JsonReader json);

    bool operator==(const AccessUser& other) const;
    bool operator!=(const AccessUser& other) const { return !(*this == other); }

private:
    UserId m_id;
    QString m_name;
    AccessRole m_role;
    QList<AccessDevice> m_devices;
};

/// Who a user invite is for: the role its joiner takes and a label naming them.
struct InvitedUser
{
    AccessRole role;
    QString label;

    bool operator==(const InvitedUser& other) const { return role == other.role && label == other.label; }
};

/// An open invite: a one-time bearer key the data key is sealed to until it is used or
/// withdrawn. A user invite names the user it is for; a device invite names none, and joins
/// as one of its maker's devices.
class AccessInvite
{
public:
    static AccessInvite forDevice(const UserId& maker, const QByteArray& publicKey, const QByteArray& verifyKey);
    static AccessInvite forUser(const UserId& maker, AccessRole role, const QString& label,
                                const QByteArray& publicKey, const QByteArray& verifyKey);

    const UserId& maker() const { return m_maker; }

    /// The user a user invite is for; empty for a device invite.
    const std::optional<InvitedUser>& invitedUser() const { return m_invitedUser; }

    const QByteArray& publicKey() const { return m_publicKey; }
    const QByteArray& verifyKey() const { return m_verifyKey; }

    QJsonObject toJson() const;
    static AccessInvite fromJson(JsonReader json);

    bool operator==(const AccessInvite& other) const;
    bool operator!=(const AccessInvite& other) const { return !(*this == other); }

private:
    AccessInvite(const UserId& maker, const std::optional<InvitedUser>& invitedUser, const QByteArray& publicKey,
                 const QByteArray& verifyKey);

    UserId m_maker;
    std::optional<InvitedUser> m_invitedUser;
    QByteArray m_publicKey;
    QByteArray m_verifyKey;
};

/// The public keys of one recipient, a device or an open invite.
struct RecipientKeys
{
    QByteArray publicKey;
    QByteArray verifyKey;
};

/// Who may read the ward: its users and their devices, its open invites, and a hash of every
/// data key it used before its current one.
class AccessList
{
public:
    AccessList() = default;
    AccessList(const QList<AccessUser>& users, const QList<AccessInvite>& invites, const QList<QByteArray>& formerKeys);

    /// A new ward's list: its creator, an admin, with this device and nobody else.
    static AccessList founding(const UserId& user, const QString& userName, const QString& deviceName,
                               const Identity& device);

    const QList<AccessUser>& users() const { return m_users; }
    const QList<AccessInvite>& invites() const { return m_invites; }
    const QList<QByteArray>& formerKeys() const { return m_formerKeys; }

    /// Every device of every user, then every open invite: exactly the keys every write seals
    /// the data key to.
    QList<RecipientKeys> recipients() const;

    QJsonObject toJson() const;
    static AccessList fromJson(JsonReader json);

    bool operator==(const AccessList& other) const;
    bool operator!=(const AccessList& other) const { return !(*this == other); }

private:
    QList<AccessUser> m_users;
    QList<AccessInvite> m_invites;
    QList<QByteArray> m_formerKeys;
};
