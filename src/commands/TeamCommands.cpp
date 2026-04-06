#include "TeamCommands.h"
#include "Document.h"

// ============================================================================
// AddTeamCommand
// ============================================================================

AddTeamCommand::AddTeamCommand(const Team& team)
    : m_team(team)
{
}

void AddTeamCommand::execute(Document& document)
{
    document.addTeam(m_team);
}

void AddTeamCommand::undo(Document& document)
{
    document.removeTeam(m_team.id());
}

QString AddTeamCommand::description() const
{
    return QObject::tr("Add Team \"%1\"").arg(m_team.name());
}

DocumentChange AddTeamCommand::documentChange() const
{
    return DocumentChange::team().added(m_team.id());
}

// ============================================================================
// UpdateTeamCommand
// ============================================================================

UpdateTeamCommand::UpdateTeamCommand(const Team& oldTeam, const Team& newTeam)
    : m_oldTeam(oldTeam)
    , m_newTeam(newTeam)
{
}

void UpdateTeamCommand::execute(Document& document)
{
    document.updateTeam(m_newTeam);
}

void UpdateTeamCommand::undo(Document& document)
{
    document.updateTeam(m_oldTeam);
}

QString UpdateTeamCommand::description() const
{
    return QObject::tr("Update Team \"%1\"").arg(m_oldTeam.name());
}

DocumentChange UpdateTeamCommand::documentChange() const
{
    return DocumentChange::team().updated(m_newTeam.id());
}

// ============================================================================
// DeleteTeamCommand
// ============================================================================

DeleteTeamCommand::DeleteTeamCommand(const Team& team)
    : m_team(team)
{
}

void DeleteTeamCommand::execute(Document& document)
{
    document.removeTeam(m_team.id());
}

void DeleteTeamCommand::undo(Document& document)
{
    document.addTeam(m_team);
}

QString DeleteTeamCommand::description() const
{
    return QObject::tr("Delete Team \"%1\"").arg(m_team.name());
}

DocumentChange DeleteTeamCommand::documentChange() const
{
    return DocumentChange::team().removed(m_team.id());
}

// ============================================================================
// AddTeamMemberCommand
// ============================================================================

AddTeamMemberCommand::AddTeamMemberCommand(const TeamId& teamId, const PersonId& memberId)
    : m_teamId(teamId)
    , m_memberId(memberId)
{
}

void AddTeamMemberCommand::execute(Document& document)
{
    document.addMemberToTeam(m_teamId, m_memberId);
}

void AddTeamMemberCommand::undo(Document& document)
{
    document.removeMemberFromTeam(m_teamId, m_memberId);
}

QString AddTeamMemberCommand::description() const
{
    return QObject::tr("Add Member to Team");
}

DocumentChange AddTeamMemberCommand::documentChange() const
{
    return DocumentChange::team().updated(m_teamId);
}

// ============================================================================
// RemoveTeamMemberCommand
// ============================================================================

RemoveTeamMemberCommand::RemoveTeamMemberCommand(const TeamId& teamId, const PersonId& memberId)
    : m_teamId(teamId)
    , m_memberId(memberId)
{
}

void RemoveTeamMemberCommand::execute(Document& document)
{
    document.removeMemberFromTeam(m_teamId, m_memberId);
}

void RemoveTeamMemberCommand::undo(Document& document)
{
    document.addMemberToTeam(m_teamId, m_memberId);
}

QString RemoveTeamMemberCommand::description() const
{
    return QObject::tr("Remove Member from Team");
}

DocumentChange RemoveTeamMemberCommand::documentChange() const
{
    return DocumentChange::team().updated(m_teamId);
}
