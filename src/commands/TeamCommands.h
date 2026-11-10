#pragma once

#include "Command.h"
#include "FieldChange.h"
#include "ReferenceCleanup.h"
#include "Team.h"

#include <optional>

/// A new team, or a deleted one put back. Its start condition is that every member exists;
/// its result holds when a team with its id is already there. It also restores the tasks
/// its cleanup holds to the team, where they still stand as the deletion left them.
class AddTeamCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "addTeam";

    /// The command that adds team, naming its members as document holds them.
    static UndoableCommandPtr fromNew(const Team& team, const Document& document);

    AddTeamCommand(const Team& team, const ReferenceCleanup& cleanup, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    CommandPtr inverse() const override;
    QJsonObject toJson() const override;

    /// The whole document when it restores tasks, otherwise the team.
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    Team m_team;
    ReferenceCleanup m_cleanup;
};

/// An edit to one team, built from the difference between the team as opened and as saved:
/// its name and leader each judged on its own, and its members as the ids added and the ids
/// removed. A team's leader is one of its members, so an edit conflicts when the team it
/// would leave has a leader who is not, and every person it adds must exist.
class UpdateTeamCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "updateTeam";

    /// The edit from the team as opened to the team as saved, naming people as document
    /// holds them, or an empty pointer when nothing differs, which WardSession::commit takes
    /// as nothing to do.
    static UndoableCommandPtr fromEdit(const Team& opened, const Team& saved, const Document& document);

    UpdateTeamCommand(const TeamId& teamId,
                      const std::optional<FieldChange<QString>>& name,
                      const std::optional<FieldChange<std::optional<PersonId>>>& leader,
                      const IdSetChange<PersonId>& members,
                      const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    CommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    TeamId m_teamId;
    std::optional<FieldChange<QString>> m_name;
    std::optional<FieldChange<std::optional<PersonId>>> m_leader;
    IdSetChange<PersonId> m_members;
};

/// A team deleted, held as last seen: a change to the team since makes the deletion
/// conflict, and the team being gone already means its result holds. It unassigns every task
/// assigned to the team when it runs.
class DeleteTeamCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "deleteTeam";

    /// The command that deletes team as the user sees it, naming its members as document
    /// holds them, and holding the tasks in document assigned to it.
    static UndoableCommandPtr fromSeen(const Team& team, const Document& document);

    DeleteTeamCommand(const Team& team, const ReferenceCleanup& cleanup, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    CommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    Team m_team;
    ReferenceCleanup m_cleanup;
};
