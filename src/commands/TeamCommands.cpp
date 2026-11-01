#include "TeamCommands.h"
#include "Document.h"
#include "FieldChangeJson.h"

#include <QObject>

namespace
{

QString leaderText(const std::optional<PersonId>& leader, const Document& document)
{
    if (!leader.has_value())
    {
        return QObject::tr("(none)");
    }
    std::optional<Person> person = document.findPersonById(*leader);
    return person.has_value() ? person->displayName() : leader->toString();
}

}  // namespace

// ----------------------------------------------------------------------------
// AddTeamCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr AddTeamCommand::fromNew(const Team& team, const Document& document)
{
    CommandDescription description(QObject::tr("New team: %1").arg(team.name()),
                                   describePeopleChange(IdSetChange<PersonId>(team.memberIds(), {}), document));
    return std::make_shared<AddTeamCommand>(team, description);
}

AddTeamCommand::AddTeamCommand(const Team& team, const CommandDescription& description)
    : UndoableCommand(description)
    , m_team(team)
{
}

CommandOutcome AddTeamCommand::execute(Document& document) const
{
    if (document.findTeamById(m_team.id()).has_value())
    {
        return CommandOutcome::Applied;
    }
    if (!everyPersonExists(m_team.memberIds(), document))
    {
        return CommandOutcome::Conflicted;
    }
    document.addTeam(m_team);
    return CommandOutcome::Applied;
}

UndoableCommandPtr AddTeamCommand::inverse() const
{
    return std::make_shared<DeleteTeamCommand>(m_team, description().undone());
}

QJsonObject AddTeamCommand::toJson() const
{
    QJsonObject json;
    json["team"] = m_team.toJson();
    return json;
}

DocumentChange AddTeamCommand::documentChange() const
{
    return DocumentChange::team().updated(m_team.id());
}

CommandPtr AddTeamCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<AddTeamCommand>(Team::fromJson(json["team"].toObject()), description);
}

// ----------------------------------------------------------------------------
// UpdateTeamCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr UpdateTeamCommand::fromEdit(const Team& opened, const Team& saved, const Document& document)
{
    std::optional<FieldChange<QString>> name = changeBetween(opened.name(), saved.name());
    std::optional<FieldChange<std::optional<PersonId>>> leader = changeBetween(opened.leaderId(), saved.leaderId());
    IdSetChange<PersonId> members = IdSetChange<PersonId>::between(opened.memberIds(), saved.memberIds());
    if (!name.has_value() && !leader.has_value() && members.isEmpty())
    {
        return nullptr;
    }

    QStringList lines;
    if (name.has_value())
    {
        lines.append(QObject::tr("name: %1").arg(name->after()));
    }
    if (leader.has_value())
    {
        lines.append(QObject::tr("leader: %1").arg(leaderText(leader->after(), document)));
    }
    lines.append(describePeopleChange(members, document));
    CommandDescription description(QObject::tr("%1 team").arg(opened.name()), lines);
    return std::make_shared<UpdateTeamCommand>(opened.id(), name, leader, members, description);
}

UpdateTeamCommand::UpdateTeamCommand(const TeamId& teamId,
                                     const std::optional<FieldChange<QString>>& name,
                                     const std::optional<FieldChange<std::optional<PersonId>>>& leader,
                                     const IdSetChange<PersonId>& members,
                                     const CommandDescription& description)
    : UndoableCommand(description)
    , m_teamId(teamId)
    , m_name(name)
    , m_leader(leader)
    , m_members(members)
{
}

CommandOutcome UpdateTeamCommand::execute(Document& document) const
{
    std::optional<Team> team = document.findTeamById(m_teamId);
    if (!team.has_value()
        || !changeHolds(m_name, team->name())
        || !changeHolds(m_leader, team->leaderId())
        || !everyPersonExists(m_members.added(), document))
    {
        return CommandOutcome::Conflicted;
    }

    QSet<PersonId> members = m_members.appliedTo(team->memberIds());
    std::optional<PersonId> leader = changedValue(m_leader, team->leaderId());
    if (leader.has_value() && !members.contains(*leader))
    {
        return CommandOutcome::Conflicted;
    }

    Team updated = *team;
    updated.setName(changedValue(m_name, team->name()));
    updated.setMemberIds(members);
    updated.setLeaderId(leader);
    document.updateTeam(updated);
    return CommandOutcome::Applied;
}

UndoableCommandPtr UpdateTeamCommand::inverse() const
{
    return std::make_shared<UpdateTeamCommand>(m_teamId,
                                               changeSwapped(m_name),
                                               changeSwapped(m_leader),
                                               m_members.swapped(),
                                               description().undone());
}

QJsonObject UpdateTeamCommand::toJson() const
{
    QJsonObject json;
    json["teamId"] = m_teamId.toString();
    writeChange(json, "name", m_name);
    writeChange(json, "leader", m_leader);
    json["members"] = peopleChangeToJson(m_members);
    return json;
}

DocumentChange UpdateTeamCommand::documentChange() const
{
    return DocumentChange::team().updated(m_teamId);
}

CommandPtr UpdateTeamCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<UpdateTeamCommand>(TeamId::fromString(json["teamId"].toString()),
                                               readChange<QString>(json, "name"),
                                               readChange<std::optional<PersonId>>(json, "leader"),
                                               peopleChangeFromJson(json["members"].toObject()),
                                               description);
}

// ----------------------------------------------------------------------------
// DeleteTeamCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr DeleteTeamCommand::fromSeen(const Team& team, const Document& document)
{
    CommandDescription description(QObject::tr("Delete team: %1").arg(team.name()),
                                   describePeopleChange(IdSetChange<PersonId>({}, team.memberIds()), document));
    return std::make_shared<DeleteTeamCommand>(team, description);
}

DeleteTeamCommand::DeleteTeamCommand(const Team& team, const CommandDescription& description)
    : UndoableCommand(description)
    , m_team(team)
{
}

CommandOutcome DeleteTeamCommand::execute(Document& document) const
{
    std::optional<Team> team = document.findTeamById(m_team.id());
    if (!team.has_value())
    {
        return CommandOutcome::Applied;
    }
    if (*team != m_team)
    {
        return CommandOutcome::Conflicted;
    }
    document.removeTeam(m_team.id());
    return CommandOutcome::Applied;
}

UndoableCommandPtr DeleteTeamCommand::inverse() const
{
    return std::make_shared<AddTeamCommand>(m_team, description().undone());
}

QJsonObject DeleteTeamCommand::toJson() const
{
    QJsonObject json;
    json["team"] = m_team.toJson();
    return json;
}

DocumentChange DeleteTeamCommand::documentChange() const
{
    return DocumentChange::team().updated(m_team.id());
}

CommandPtr DeleteTeamCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<DeleteTeamCommand>(Team::fromJson(json["team"].toObject()), description);
}
