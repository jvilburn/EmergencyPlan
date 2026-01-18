#pragma once

#include "Command.h"
#include "SpecialNeed.h"
#include <optional>

class SetSpecialNeedCommand : public Command
{
public:
    // Factory methods
    static SetSpecialNeedCommand forPerson(const QString& personId, const QString& note);
    static SetSpecialNeedCommand forFamily(const QString& familyId, const QString& note);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    SetSpecialNeedCommand(std::optional<QString> personId, std::optional<QString> familyId, const QString& note);

    std::optional<QString> m_personId;
    std::optional<QString> m_familyId;
    QString m_note;
    std::optional<SpecialNeed> m_previousNeed;  // For undo
};

class ClearSpecialNeedCommand : public Command
{
public:
    static ClearSpecialNeedCommand forPerson(const QString& personId);
    static ClearSpecialNeedCommand forFamily(const QString& familyId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    ClearSpecialNeedCommand(std::optional<QString> personId, std::optional<QString> familyId);

    std::optional<QString> m_personId;
    std::optional<QString> m_familyId;
    std::optional<SpecialNeed> m_removedNeed;  // For undo
};
