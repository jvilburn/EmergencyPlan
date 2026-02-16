#pragma once

#include "Command.h"
#include "Team.h"

class AddTeamCommand : public Command
{
public:
    explicit AddTeamCommand(const Team& team);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Team m_team;
};

class UpdateTeamCommand : public Command
{
public:
    UpdateTeamCommand(const Team& oldTeam, const Team& newTeam);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Team m_oldTeam;
    Team m_newTeam;
};

class DeleteTeamCommand : public Command
{
public:
    explicit DeleteTeamCommand(const Team& team);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Team m_team;
};

class AddTeamMemberCommand : public Command
{
public:
    AddTeamMemberCommand(const TeamId& teamId, const PersonId& memberId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    TeamId m_teamId;
    PersonId m_memberId;
};

class RemoveTeamMemberCommand : public Command
{
public:
    RemoveTeamMemberCommand(const TeamId& teamId, const PersonId& memberId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    TeamId m_teamId;
    PersonId m_memberId;
};
