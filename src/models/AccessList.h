#pragma once

#include "Id.h"
#include "JsonReader.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <optional>

class Identity;

enum class AccessRole
{
    Admin,
    Member
};

/// The word role is written as: admin or member.
QString accessRoleName(AccessRole role);

/// The role under key, read as one of the words accessRoleName writes.
AccessRole readAccessRole(JsonReader& json, const QString& key);

/// One of a user's devices, by the keys the data key is sealed to and its writes are
/// signed with.
class AccessDevice
{
public:
    AccessDevice(const QString& name, const QByteArray& publicKey, const QByteArray& verifyKey);

    const QString& name() const { return m_name; }
    const QByteArray& publicKey() const { return m_publicKey; }
    const QByteArray& verifyKey() const { return m_verifyKey; }

    /// The fingerprint of the two public keys: what a header names this device by.
    QByteArray fingerprint() const;

    void setName(const QString& name) { m_name = name; }

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

    void setName(const QString& name) { m_name = name; }
    void setRole(AccessRole role) { m_role = role; }
    void setDevices(const QList<AccessDevice>& devices) { m_devices = devices; }

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

    /// The fingerprint of the two public keys: what a header names this invite by.
    QByteArray fingerprint() const;

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

    /// What a former data key is kept as: 32-byte BLAKE2b of the key, personalized
    /// wp-former-key.
    static QByteArray formerKeyHash(const QByteArray& dataKey);

    const QList<AccessUser>& users() const { return m_users; }
    const QList<AccessInvite>& invites() const { return m_invites; }
    const QList<QByteArray>& formerKeys() const { return m_formerKeys; }

    /// Every device of every user, then every open invite: exactly the keys every write seals
    /// the data key to.
    QList<RecipientKeys> recipients() const;

    /// The fingerprints of recipients(): whom a write seals the data key to.
    QSet<QByteArray> recipientFingerprints() const;

    /// The fingerprints of every device of every user.
    QSet<QByteArray> deviceFingerprints() const;

    /// The verify key of the device or open invite whose fingerprint is fingerprint, or none
    /// when the list holds neither.
    std::optional<QByteArray> verifyKeyOf(const QByteArray& fingerprint) const;

    std::optional<AccessUser> findUser(const UserId& id) const;

    /// The user holding the device whose fingerprint is device, or none.
    std::optional<AccessUser> userOfDevice(const QByteArray& device) const;

    std::optional<AccessDevice> findDevice(const QByteArray& device) const;
    std::optional<AccessInvite> findInvite(const QByteArray& invite) const;

    /// The admins' names, sorted without regard to case.
    QStringList adminNames() const;

    /// How the interface names an open invite: "for Bob" for a user invite labelled Bob, "a
    /// device for Alice" for a device invite Alice made.
    QString describeInvite(const AccessInvite& invite) const;

    /// Replaces the user who has user's id with user, or adds user at the end when there is
    /// none.
    void setUser(const AccessUser& user);

    /// Takes the user out, with every device they hold and every invite they made.
    void removeUser(const UserId& id);

    /// Takes the device whose fingerprint is device out of whichever user holds it.
    void removeDevice(const QByteArray& device);

    void addInvite(const AccessInvite& invite) { m_invites.append(invite); }

    /// Takes the open invite whose fingerprint is invite out.
    void removeInvite(const QByteArray& invite);

    void setFormerKeys(const QList<QByteArray>& formerKeys) { m_formerKeys = formerKeys; }

    QJsonObject toJson() const;
    static AccessList fromJson(JsonReader json);

    bool operator==(const AccessList& other) const;
    bool operator!=(const AccessList& other) const { return !(*this == other); }

private:
    QList<AccessUser> m_users;
    QList<AccessInvite> m_invites;
    QList<QByteArray> m_formerKeys;
};
