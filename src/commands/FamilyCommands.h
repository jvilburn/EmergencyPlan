#pragma once

#include "Command.h"
#include "FamilyChange.h"
#include "ReferenceCleanup.h"

/// An edit to one family, built from the difference between what the edit opened with and
/// what it saved. Holds the before and after of the changed fields only, and the cleanups of
/// the members it removes and adds.
class UpdateFamilyCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "updateFamily";

    /// The edit from the family as opened to the family as saved, or an empty pointer when
    /// nothing differs, which WardSession::commit takes as nothing to do. The cleanups hold
    /// the places in document that refer to the members it removes.
    static UndoableCommandPtr fromEdit(const Family& opened, const Family& saved, const Document& document);

    UpdateFamilyCommand(const FamilyId& familyId, const FamilyChange& change, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;

    /// The whole document when the change reaches places outside the family, otherwise the
    /// family.
    DocumentChange documentChange() const override;

    /// The address after, when the change sets a non-empty address.
    QList<Address> addressesGiven() const override;

    QStringList skippedOn(const Document& document) const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    FamilyId m_familyId;
    FamilyChange m_change;
};

/// A family deleted, held as last seen: a change to the family since makes the deletion
/// conflict, and the family being gone already means its result holds. It takes the family
/// and its people out of every place that holds them when it runs.
class DeleteFamilyCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "deleteFamily";

    /// The command that deletes family as the user sees it, holding the places in document
    /// that refer to it and its people.
    static UndoableCommandPtr fromSeen(const Family& family, const Document& document);

    DeleteFamilyCommand(const Family& family, const ReferenceCleanup& cleanup, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    Family m_family;
    ReferenceCleanup m_cleanup;
};

/// A deleted family put back, with its people restored to the places the deletion held that
/// still stand as it left them. Its result holds when the family is there; it never
/// conflicts.
class RestoreFamilyCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "restoreFamily";

    RestoreFamilyCommand(const Family& family, const ReferenceCleanup& cleanup, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    /// The family's address, when it is not empty.
    QList<Address> addressesGiven() const override;

    QStringList skippedOn(const Document& document) const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    Family m_family;
    ReferenceCleanup m_cleanup;
};
