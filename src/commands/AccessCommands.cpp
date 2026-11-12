#include "AccessCommands.h"
#include "AccessRules.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "Document.h"
#include "FieldChangeJson.h"
#include "Identity.h"

#include <QObject>

// JSON for the user details SetUserDetailsCommand changes, read and written through the
// templates in FieldChangeJson.h: an object holding name and email.

QJsonValue toJsonValue(const UserDetails& value)
{
    QJsonObject json;
    json["name"] = value.name;
    json["email"] = value.email;
    return json;
}

template<>
UserDetails readValue<UserDetails>(JsonReader& json, const QString& key)
{
    JsonReader details = json.object(key);
    UserDetails value{details.string("name"), details.string("email")};
    details.finish();
    return value;
}

namespace
{

QByteArray readActor(JsonReader& json)
{
    return json.bytes("actor", Crypto::FingerprintBytes);
}

UserId readUserId(JsonReader& json, const QString& key)
{
    return UserId::fromBytes(json.bytes(key, Crypto::IdBytes));
}

}  // namespace

// ============================================================================
// InviteCommand
// ============================================================================

CommandPtr InviteCommand::create(const QByteArray& actor, const AccessList& access,
                                 const std::optional<InvitedUser>& user, const Identity& keys)
{
    UserId maker = access.userOfDevice(actor)->id();
    if (user.has_value())
    {
        AccessInvite invite = AccessInvite::forUser(maker, user->role, user->label, keys.publicKey(), keys.verifyKey());
        CommandDescription description(QObject::tr("Add a user: %1").arg(user->label), {});
        return std::make_shared<InviteCommand>(actor, invite, description);
    }
    AccessInvite invite = AccessInvite::forDevice(maker, keys.publicKey(), keys.verifyKey());
    return std::make_shared<InviteCommand>(actor, invite, CommandDescription(QObject::tr("Add a device"), {}));
}

InviteCommand::InviteCommand(const QByteArray& actor, const AccessInvite& invite, const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_invite(invite)
{
}

CommandOutcome InviteCommand::execute(Document& document) const
{
    AccessList access = document.access();
    if (!access.findInvite(m_invite.fingerprint()).has_value())
    {
        access.addInvite(m_invite);
        document.setAccess(access);
    }
    return CommandOutcome::Applied;
}

QString InviteCommand::refusal(const Document& document) const
{
    return AccessRules::invite(document.access(), m_actor, m_invite.invitedUser().has_value());
}

QJsonObject InviteCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["invite"] = m_invite.toJson();
    return json;
}

DocumentChange InviteCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr InviteCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    AccessInvite invite = AccessInvite::fromJson(json.object("invite"));
    json.finish();
    return std::make_shared<InviteCommand>(actor, invite, description);
}

// ============================================================================
// RemoveUserCommand
// ============================================================================

CommandPtr RemoveUserCommand::create(const QByteArray& actor, const AccessUser& user)
{
    CommandDescription description(QObject::tr("Remove %1").arg(user.name()), {});
    return std::make_shared<RemoveUserCommand>(actor, user.id(), description);
}

RemoveUserCommand::RemoveUserCommand(const QByteArray& actor, const UserId& userId,
                                     const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_userId(userId)
{
}

CommandOutcome RemoveUserCommand::execute(Document& document) const
{
    AccessList access = document.access();
    access.removeUser(m_userId);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString RemoveUserCommand::refusal(const Document& document) const
{
    return AccessRules::removeUser(document.access(), m_actor, m_userId);
}

QJsonObject RemoveUserCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["user"] = m_userId.toString();
    return json;
}

DocumentChange RemoveUserCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr RemoveUserCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    UserId user = readUserId(json, "user");
    json.finish();
    return std::make_shared<RemoveUserCommand>(actor, user, description);
}

// ============================================================================
// RemoveDeviceCommand
// ============================================================================

CommandPtr RemoveDeviceCommand::create(const QByteArray& actor, const AccessDevice& device)
{
    CommandDescription description(QObject::tr("Remove device %1").arg(device.name()), {});
    return std::make_shared<RemoveDeviceCommand>(actor, device.fingerprint(), description);
}

RemoveDeviceCommand::RemoveDeviceCommand(const QByteArray& actor, const QByteArray& device,
                                         const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_device(device)
{
}

CommandOutcome RemoveDeviceCommand::execute(Document& document) const
{
    AccessList access = document.access();
    access.removeDevice(m_device);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString RemoveDeviceCommand::refusal(const Document& document) const
{
    return AccessRules::removeDevice(document.access(), m_actor, m_device);
}

QJsonObject RemoveDeviceCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["device"] = Base64Url::encode(m_device);
    return json;
}

DocumentChange RemoveDeviceCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr RemoveDeviceCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    QByteArray device = json.bytes("device", Crypto::FingerprintBytes);
    json.finish();
    return std::make_shared<RemoveDeviceCommand>(actor, device, description);
}

// ============================================================================
// WithdrawInviteCommand
// ============================================================================

CommandPtr WithdrawInviteCommand::create(const QByteArray& actor, const AccessInvite& invite, const AccessList& access)
{
    QString heading = invite.invitedUser().has_value()
        ? QObject::tr("Withdraw the invite for %1").arg(invite.invitedUser()->label)
        : QObject::tr("Withdraw the device invite from %1").arg(access.findUser(invite.maker())->name());
    return std::make_shared<WithdrawInviteCommand>(actor, invite.fingerprint(), CommandDescription(heading, {}));
}

WithdrawInviteCommand::WithdrawInviteCommand(const QByteArray& actor, const QByteArray& invite,
                                             const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_invite(invite)
{
}

CommandOutcome WithdrawInviteCommand::execute(Document& document) const
{
    AccessList access = document.access();
    access.removeInvite(m_invite);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString WithdrawInviteCommand::refusal(const Document& document) const
{
    return AccessRules::withdrawInvite(document.access(), m_actor, m_invite);
}

QJsonObject WithdrawInviteCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["invite"] = Base64Url::encode(m_invite);
    return json;
}

DocumentChange WithdrawInviteCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr WithdrawInviteCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    QByteArray invite = json.bytes("invite", Crypto::FingerprintBytes);
    json.finish();
    return std::make_shared<WithdrawInviteCommand>(actor, invite, description);
}

// ============================================================================
// SetRoleCommand
// ============================================================================

CommandPtr SetRoleCommand::create(const QByteArray& actor, const AccessUser& user, AccessRole role)
{
    QString heading = role == AccessRole::Admin
        ? QObject::tr("Make %1 an admin").arg(user.name())
        : QObject::tr("Make %1 a member").arg(user.name());
    return std::make_shared<SetRoleCommand>(actor, user.id(), role, CommandDescription(heading, {}));
}

SetRoleCommand::SetRoleCommand(const QByteArray& actor, const UserId& userId, AccessRole role,
                               const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_userId(userId)
    , m_role(role)
{
}

CommandOutcome SetRoleCommand::execute(Document& document) const
{
    AccessList access = document.access();
    std::optional<AccessUser> user = access.findUser(m_userId);
    if (!user.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    user->setRole(m_role);
    access.setUser(*user);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString SetRoleCommand::refusal(const Document& document) const
{
    return AccessRules::setRole(document.access(), m_actor, m_userId, m_role);
}

QJsonObject SetRoleCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["user"] = m_userId.toString();
    json["role"] = accessRoleName(m_role);
    return json;
}

DocumentChange SetRoleCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr SetRoleCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    UserId user = readUserId(json, "user");
    AccessRole role = readAccessRole(json, "role");
    json.finish();
    return std::make_shared<SetRoleCommand>(actor, user, role, description);
}

// ============================================================================
// SetUserDetailsCommand
// ============================================================================

CommandPtr SetUserDetailsCommand::create(const QByteArray& actor, const AccessList& access,
                                         const UserDetails& details)
{
    UserDetails current = access.userOfDevice(actor)->details();
    if (current == details)
    {
        return nullptr;
    }
    QString email = details.email.isEmpty() ? QObject::tr("No email") : QObject::tr("Email: %1").arg(details.email);
    CommandDescription description(QObject::tr("Change your name or email"),
                                   {QObject::tr("Name: %1").arg(details.name), email});
    return std::make_shared<SetUserDetailsCommand>(actor, FieldChange<UserDetails>(current, details), description);
}

SetUserDetailsCommand::SetUserDetailsCommand(const QByteArray& actor, const FieldChange<UserDetails>& details,
                                             const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_details(details)
{
}

CommandOutcome SetUserDetailsCommand::execute(Document& document) const
{
    AccessList access = document.access();
    std::optional<AccessUser> user = access.userOfDevice(m_actor);
    if (!user.has_value() || !m_details.holds(user->details()))
    {
        return CommandOutcome::Conflicted;
    }
    user->setDetails(m_details.after());
    access.setUser(*user);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QJsonObject SetUserDetailsCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["details"] = changeToJson(m_details);
    return json;
}

DocumentChange SetUserDetailsCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr SetUserDetailsCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    FieldChange<UserDetails> details = changeFromJson<UserDetails>(json.object("details"));
    json.finish();
    return std::make_shared<SetUserDetailsCommand>(actor, details, description);
}

// ============================================================================
// RenameDeviceCommand
// ============================================================================

CommandPtr RenameDeviceCommand::create(const QByteArray& actor, const AccessList& access, const QString& name)
{
    QString current = access.findDevice(actor)->name();
    if (current == name)
    {
        return nullptr;
    }
    return std::make_shared<RenameDeviceCommand>(
        actor, FieldChange<QString>(current, name),
        CommandDescription(QObject::tr("Rename this device to %1").arg(name), {}));
}

RenameDeviceCommand::RenameDeviceCommand(const QByteArray& actor, const FieldChange<QString>& name,
                                         const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_name(name)
{
}

CommandOutcome RenameDeviceCommand::execute(Document& document) const
{
    AccessList access = document.access();
    AccessUser user = *access.userOfDevice(m_actor);
    QList<AccessDevice> devices = user.devices();
    for (AccessDevice& device : devices)
    {
        if (device.fingerprint() == m_actor)
        {
            if (!m_name.holds(device.name()))
            {
                return CommandOutcome::Conflicted;
            }
            device.setName(m_name.after());
        }
    }
    user.setDevices(devices);
    access.setUser(user);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QJsonObject RenameDeviceCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["name"] = changeToJson(m_name);
    return json;
}

DocumentChange RenameDeviceCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr RenameDeviceCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    FieldChange<QString> name = changeFromJson<QString>(json.object("name"));
    json.finish();
    return std::make_shared<RenameDeviceCommand>(actor, name, description);
}

// ============================================================================
// JoinCommand
// ============================================================================

CommandPtr JoinCommand::create(const AccessInvite& invite, const AccessDevice& device, const UserDetails& user,
                               const QString& wardName)
{
    std::optional<JoiningUser> joining;
    if (invite.invitedUser().has_value())
    {
        joining = JoiningUser{UserId::generate(), user};
    }
    return std::make_shared<JoinCommand>(invite, device, joining,
                                         CommandDescription(QObject::tr("Join %1").arg(wardName), {}));
}

CommandPtr JoinCommand::rejoin(const AccessInvite& invite, const AccessList& held, const QByteArray& device,
                               const QString& wardName)
{
    std::optional<JoiningUser> user;
    if (invite.invitedUser().has_value())
    {
        AccessUser joined = *held.userOfDevice(device);
        user = JoiningUser{joined.id(), joined.details()};
    }
    return std::make_shared<JoinCommand>(invite, *held.findDevice(device), user,
                                         CommandDescription(QObject::tr("Join %1").arg(wardName), {}));
}

JoinCommand::JoinCommand(const AccessInvite& invite, const AccessDevice& device,
                         const std::optional<JoiningUser>& user, const CommandDescription& description)
    : Command(description)
    , m_invite(invite)
    , m_device(device)
    , m_user(user)
{
}

CommandOutcome JoinCommand::execute(Document& document) const
{
    AccessList access = document.access();
    if (access.findDevice(m_device.fingerprint()).has_value())
    {
        return CommandOutcome::Applied;
    }
    access.removeInvite(m_invite.fingerprint());
    if (m_user.has_value())
    {
        access.setUser(AccessUser(m_user->id, m_user->details, m_invite.invitedUser()->role, {m_device}));
    }
    else
    {
        AccessUser maker = *access.findUser(m_invite.maker());
        QList<AccessDevice> devices = maker.devices();
        devices.append(m_device);
        maker.setDevices(devices);
        access.setUser(maker);
    }
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString JoinCommand::refusal(const Document& document) const
{
    return AccessRules::join(document.access(), m_invite.fingerprint(), m_device.fingerprint());
}

QJsonObject JoinCommand::toJson() const
{
    QJsonObject json;
    json["invite"] = m_invite.toJson();
    json["device"] = m_device.toJson();
    if (m_user.has_value())
    {
        QJsonObject user;
        user["id"] = m_user->id.toString();
        user["name"] = m_user->details.name;
        user["email"] = m_user->details.email;
        json["user"] = user;
    }
    return json;
}

DocumentChange JoinCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr JoinCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    AccessInvite invite = AccessInvite::fromJson(json.object("invite"));
    AccessDevice device = AccessDevice::fromJson(json.object("device"));
    std::optional<JoiningUser> user;
    if (invite.invitedUser().has_value())
    {
        JsonReader userJson = json.object("user");
        UserId id = readUserId(userJson, "id");
        QString name = userJson.string("name");
        QString email = userJson.string("email");
        userJson.finish();
        user = JoiningUser{id, UserDetails{name, email}};
    }
    json.finish();
    return std::make_shared<JoinCommand>(invite, device, user, description);
}
