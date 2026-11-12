#pragma once

#include "AccessList.h"
#include "Command.h"
#include "FieldChange.h"
#include "JsonReader.h"

#include <QByteArray>
#include <optional>

class Identity;

/// Adds an open invite, made by the user of the device actor. Its result holds when the invite
/// is there already. Its rule: a user invite is for admins only.
class InviteCommand : public Command
{
public:
    static constexpr const char* TypeName = "invite";

    /// An invite under keys, made on the device actor in access: for user when it is set,
    /// otherwise for one of the maker's devices.
    static CommandPtr create(const QByteArray& actor, const AccessList& access, const std::optional<InvitedUser>& user,
                             const Identity& keys);

    InviteCommand(const QByteArray& actor, const AccessInvite& invite, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    AccessInvite m_invite;
};

/// Removes a user: their entry, every device it holds by then, and every invite they have made
/// by then. It never conflicts, since its intent is that this person is out: a device they
/// added or an invite they made meanwhile goes with them, and a user already gone is its result
/// already holding. The version that carries it rotates the data key.
class RemoveUserCommand : public Command
{
public:
    static constexpr const char* TypeName = "removeUser";

    /// The removal of user, made on the device actor.
    static CommandPtr create(const QByteArray& actor, const AccessUser& user);

    RemoveUserCommand(const QByteArray& actor, const UserId& userId, const CommandDescription& description);

    const UserId& userId() const { return m_userId; }

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    UserId m_userId;
};

/// Removes one device from whichever user holds it. Its result holds when the device is gone.
/// Its rule: only one of the actor's own devices, and never their last.
class RemoveDeviceCommand : public Command
{
public:
    static constexpr const char* TypeName = "removeDevice";

    /// The removal of device, made on the device actor.
    static CommandPtr create(const QByteArray& actor, const AccessDevice& device);

    RemoveDeviceCommand(const QByteArray& actor, const QByteArray& device, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    QByteArray m_device;  ///< the fingerprint of the device removed
};

/// Takes an open invite out. Its result holds when the invite is gone. Its rule: its maker, or
/// an admin.
class WithdrawInviteCommand : public Command
{
public:
    static constexpr const char* TypeName = "withdrawInvite";

    /// The withdrawal of invite, which access holds, made on the device actor.
    static CommandPtr create(const QByteArray& actor, const AccessInvite& invite, const AccessList& access);

    WithdrawInviteCommand(const QByteArray& actor, const QByteArray& invite, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    QByteArray m_invite;  ///< the fingerprint of the invite withdrawn
};

/// Sets one user's role. It conflicts only when the user is gone, so of two role changes to one
/// user, the later stands. Its rule: admins only, and the only admin cannot step down.
class SetRoleCommand : public Command
{
public:
    static constexpr const char* TypeName = "setRole";

    /// user's role set to role on the device actor.
    static CommandPtr create(const QByteArray& actor, const AccessUser& user, AccessRole role);

    SetRoleCommand(const QByteArray& actor, const UserId& userId, AccessRole role,
                   const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    UserId m_userId;
    AccessRole m_role;
};

/// Sets the name and email of the user of the device that made it, each judged as its own
/// value, so a name set on one device and an email set on another both apply. It names no
/// other user, so a device sets the details only of its own user.
class SetUserDetailsCommand : public Command
{
public:
    static constexpr const char* TypeName = "setUserDetails";

    /// The details of the user of the device actor in access set to details, or an empty
    /// pointer when access holds them so already, which WardSession::commit takes as nothing
    /// to do.
    static CommandPtr create(const QByteArray& actor, const AccessList& access, const UserDetails& details);

    /// A change to the name, to the email, or to both.
    SetUserDetailsCommand(const QByteArray& actor, const std::optional<FieldChange<QString>>& name,
                          const std::optional<FieldChange<QString>>& email, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    std::optional<FieldChange<QString>> m_name;
    std::optional<FieldChange<QString>> m_email;
};

/// Renames the device that made it, judged per value. It names no other device, so a device
/// renames only itself.
class RenameDeviceCommand : public Command
{
public:
    static constexpr const char* TypeName = "renameDevice";

    /// The device actor in access renamed to name, or an empty pointer when access names it so
    /// already.
    static CommandPtr create(const QByteArray& actor, const AccessList& access, const QString& name);

    RenameDeviceCommand(const QByteArray& actor, const FieldChange<QString>& name, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    FieldChange<QString> m_name;
};

/// The user a device joining through a user invite becomes: a new id, and the name and email
/// they gave.
struct JoiningUser
{
    UserId id;
    UserDetails details;
};

/// Joins the ward through an open invite: the invite's entry becomes the joining device's,
/// under a new user with the invite's role for a user invite, and as one of the maker's
/// devices for a device invite. The joining device runs it on the version its invitation
/// carries, and runs it again on any version it takes that names the invite and not the
/// device. Its result holds once the list holds the device; its rule refuses it once the
/// invite is used or withdrawn and the device is not on the list.
class JoinCommand : public Command
{
public:
    static constexpr const char* TypeName = "join";

    /// Joining the ward named wardName through invite as device: under a new user with user's
    /// details for a user invite, and as one of the invite's maker's devices for a device
    /// invite, which leaves user unused.
    static CommandPtr create(const AccessInvite& invite, const AccessDevice& device, const UserDetails& user,
                             const QString& wardName);

    /// The device whose fingerprint is device joining the ward named wardName through invite
    /// again, as held holds it: under the same device entry, and for a user invite as the same
    /// user, id and details, so every copy ends with one entry for it.
    static CommandPtr rejoin(const AccessInvite& invite, const AccessList& held, const QByteArray& device,
                             const QString& wardName);

    JoinCommand(const AccessInvite& invite, const AccessDevice& device, const std::optional<JoiningUser>& user,
                const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    AccessInvite m_invite;
    AccessDevice m_device;
    std::optional<JoiningUser> m_user;  ///< for a user invite
};
