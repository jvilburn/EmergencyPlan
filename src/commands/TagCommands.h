#pragma once

#include "Command.h"
#include "Tag.h"

class AddTagCommand : public Command
{
public:
    explicit AddTagCommand(const Tag& tag);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Tag m_tag;
};

class UpdateTagCommand : public Command
{
public:
    UpdateTagCommand(const Tag& oldTag, const Tag& newTag);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Tag m_oldTag;
    Tag m_newTag;
};

class DeleteTagCommand : public Command
{
public:
    explicit DeleteTagCommand(const Tag& tag);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Tag m_tag;
};

class AssignTagToPersonCommand : public Command
{
public:
    AssignTagToPersonCommand(const TagId& tagId, const PersonId& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    TagId m_tagId;
    PersonId m_personId;
};

class AssignTagToFamilyCommand : public Command
{
public:
    AssignTagToFamilyCommand(const TagId& tagId, const FamilyId& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    TagId m_tagId;
    FamilyId m_familyId;
};

class UnassignTagFromPersonCommand : public Command
{
public:
    UnassignTagFromPersonCommand(const TagId& tagId, const PersonId& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    TagId m_tagId;
    PersonId m_personId;
};

class UnassignTagFromFamilyCommand : public Command
{
public:
    UnassignTagFromFamilyCommand(const TagId& tagId, const FamilyId& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    TagId m_tagId;
    FamilyId m_familyId;
};
