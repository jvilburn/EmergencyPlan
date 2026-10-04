#include "RosterChange.h"

#include "StrictJson.h"

namespace
{

// Part of the settings format that keeps pending changes.
const QLatin1String KeyKind("kind");
const QLatin1String KeyPerson("person");
const QLatin1String KeyDevice("device");
const QLatin1String KeyTarget("target");
const QLatin1String KeyRole("role");

const QLatin1String GrantText("grant");
const QLatin1String LinkText("link");
const QLatin1String UnlinkText("unlink");
const QLatin1String RemoveText("remove");
const QLatin1String SetRoleText("setRole");
const QLatin1String LeaveText("leave");

const QLatin1String AdminText("admin");
const QLatin1String MemberText("member");

QString kindText(RosterChange::Kind kind)
{
    switch (kind)
    {
    case RosterChange::Kind::Grant:
        return GrantText;
    case RosterChange::Kind::Link:
        return LinkText;
    case RosterChange::Kind::Unlink:
        return UnlinkText;
    case RosterChange::Kind::Remove:
        return RemoveText;
    case RosterChange::Kind::SetRole:
        return SetRoleText;
    case RosterChange::Kind::Leave:
        return LeaveText;
    }

    Q_ASSERT(false);
    return QString();
}

std::optional<RosterChange::Kind> kindFromText(const QString& text)
{
    for (RosterChange::Kind kind : {RosterChange::Kind::Grant,
                                    RosterChange::Kind::Link,
                                    RosterChange::Kind::Unlink,
                                    RosterChange::Kind::Remove,
                                    RosterChange::Kind::SetRole,
                                    RosterChange::Kind::Leave})
    {
        if (kindText(kind) == text)
        {
            return kind;
        }
    }
    return std::nullopt;
}

} // namespace

QJsonObject RosterChange::toJson() const
{
    QJsonObject json;
    json[KeyKind] = kindText(kind);

    if (person.has_value())
    {
        json[KeyPerson] = person.value().toJson();
    }
    if (device.has_value())
    {
        json[KeyDevice] = device.value().toJson();
    }
    if (target.has_value())
    {
        json[KeyTarget] = target.value().toHex();
    }
    if (role.has_value())
    {
        json[KeyRole] = role.value() == RosterPerson::Role::Admin ? AdminText : MemberText;
    }

    return json;
}

std::optional<RosterChange> RosterChange::fromJson(const QJsonObject& json)
{
    QString kind;
    RosterChange change;
    if (!StrictJson::readString(json, KeyKind, kind))
    {
        return std::nullopt;
    }

    const std::optional<Kind> parsedKind = kindFromText(kind);
    if (!parsedKind.has_value())
    {
        return std::nullopt;
    }
    change.kind = parsedKind.value();

    // The fields each kind uses, all required.
    const bool usesPerson = change.kind == Kind::Grant;
    const bool usesDevice = change.kind == Kind::Link || change.kind == Kind::Unlink;
    const bool usesTarget = change.kind == Kind::Remove || change.kind == Kind::SetRole;
    const bool usesRole = change.kind == Kind::SetRole;

    if (json.contains(KeyPerson) != usesPerson || json.contains(KeyDevice) != usesDevice
        || json.contains(KeyTarget) != usesTarget || json.contains(KeyRole) != usesRole
        || !StrictJson::hasOnlyKeys(json, {KeyKind, KeyPerson, KeyDevice, KeyTarget, KeyRole}))
    {
        return std::nullopt;
    }

    if (usesPerson)
    {
        QJsonObject personJson;
        if (!StrictJson::readObject(json, KeyPerson, personJson))
        {
            return std::nullopt;
        }
        change.person = RosterPerson::fromJson(personJson);
        if (!change.person.has_value())
        {
            return std::nullopt;
        }
    }

    if (usesDevice)
    {
        QJsonObject deviceJson;
        if (!StrictJson::readObject(json, KeyDevice, deviceJson))
        {
            return std::nullopt;
        }
        change.device = RosterDevice::fromJson(deviceJson);
        if (!change.device.has_value())
        {
            return std::nullopt;
        }
    }

    if (usesTarget)
    {
        QString target;
        if (!StrictJson::readString(json, KeyTarget, target))
        {
            return std::nullopt;
        }
        const MemberId member = MemberId::fromHex(target);
        if (!member.isValid())
        {
            return std::nullopt;
        }
        change.target = member;
    }

    if (usesRole)
    {
        QString role;
        if (!StrictJson::readString(json, KeyRole, role))
        {
            return std::nullopt;
        }
        if (role == AdminText)
        {
            change.role = RosterPerson::Role::Admin;
        }
        else if (role == MemberText)
        {
            change.role = RosterPerson::Role::Member;
        }
        else
        {
            return std::nullopt;
        }
    }

    return change;
}

bool RosterChange::operator==(const RosterChange& other) const
{
    return kind == other.kind
        && person == other.person
        && device == other.device
        && target == other.target
        && role == other.role;
}
