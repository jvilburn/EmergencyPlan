#pragma once

#include "Command.h"
#include "Family.h"

class AddFamilyCommand : public Command
{
public:
    explicit AddFamilyCommand(const Family& family);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Family m_family;
};

class UpdateFamilyCommand : public Command
{
public:
    UpdateFamilyCommand(const Family& oldFamily, const Family& newFamily);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Family m_oldFamily;
    Family m_newFamily;
};

class DeleteFamilyCommand : public Command
{
public:
    explicit DeleteFamilyCommand(const Family& family);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    Family m_family;
};

class SetFamiliesCommand : public Command
{
public:
    explicit SetFamiliesCommand(const QHash<FamilyId, Family>& families,
                                const QString& description);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QHash<FamilyId, Family> m_newFamilies;
    QHash<FamilyId, Family> m_oldFamilies;
    QString m_description;
};
