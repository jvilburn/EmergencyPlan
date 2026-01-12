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
    AssignTagToPersonCommand(const QString& tagId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_tagId;
    QString m_personId;
};

class AssignTagToFamilyCommand : public Command
{
public:
    AssignTagToFamilyCommand(const QString& tagId, const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_tagId;
    QString m_familyId;
};

class UnassignTagFromPersonCommand : public Command
{
public:
    UnassignTagFromPersonCommand(const QString& tagId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_tagId;
    QString m_personId;
};

class UnassignTagFromFamilyCommand : public Command
{
public:
    UnassignTagFromFamilyCommand(const QString& tagId, const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_tagId;
    QString m_familyId;
};
