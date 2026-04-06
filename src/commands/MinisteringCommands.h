#pragma once

#include "Command.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"

// ============================================================================
// EQ District Commands
// ============================================================================

class AddEqDistrictCommand : public Command
{
public:
    explicit AddEqDistrictCommand(const MinisteringDistrict& district);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringDistrict m_district;
};

class UpdateEqDistrictCommand : public Command
{
public:
    UpdateEqDistrictCommand(const MinisteringDistrict& oldDistrict,
                            const MinisteringDistrict& newDistrict);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringDistrict m_oldDistrict;
    MinisteringDistrict m_newDistrict;
};

class DeleteEqDistrictCommand : public Command
{
public:
    explicit DeleteEqDistrictCommand(const MinisteringDistrict& district);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringDistrict m_district;
};

// ============================================================================
// EQ Group Commands
// ============================================================================

class AddEqGroupCommand : public Command
{
public:
    explicit AddEqGroupCommand(const MinisteringGroup& group);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringGroup m_group;
};

class UpdateEqGroupCommand : public Command
{
public:
    UpdateEqGroupCommand(const MinisteringGroup& oldGroup,
                         const MinisteringGroup& newGroup);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringGroup m_oldGroup;
    MinisteringGroup m_newGroup;
};

class DeleteEqGroupCommand : public Command
{
public:
    explicit DeleteEqGroupCommand(const MinisteringGroup& group);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringGroup m_group;
};

// ============================================================================
// RS District Commands
// ============================================================================

class AddRsDistrictCommand : public Command
{
public:
    explicit AddRsDistrictCommand(const MinisteringDistrict& district);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringDistrict m_district;
};

class UpdateRsDistrictCommand : public Command
{
public:
    UpdateRsDistrictCommand(const MinisteringDistrict& oldDistrict,
                            const MinisteringDistrict& newDistrict);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringDistrict m_oldDistrict;
    MinisteringDistrict m_newDistrict;
};

class DeleteRsDistrictCommand : public Command
{
public:
    explicit DeleteRsDistrictCommand(const MinisteringDistrict& district);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringDistrict m_district;
};

// ============================================================================
// RS Group Commands
// ============================================================================

class AddRsGroupCommand : public Command
{
public:
    explicit AddRsGroupCommand(const MinisteringGroup& group);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringGroup m_group;
};

class UpdateRsGroupCommand : public Command
{
public:
    UpdateRsGroupCommand(const MinisteringGroup& oldGroup,
                         const MinisteringGroup& newGroup);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringGroup m_oldGroup;
    MinisteringGroup m_newGroup;
};

class DeleteRsGroupCommand : public Command
{
public:
    explicit DeleteRsGroupCommand(const MinisteringGroup& group);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringGroup m_group;
};
