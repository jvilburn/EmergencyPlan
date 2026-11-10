#pragma once

#include "Command.h"
#include "FamilyChange.h"
#include "ReferenceCleanup.h"

/// An edit to one family, built from the difference between what the edit opened with and
/// what it saved. Holds the before and after of the changed fields only, a ReferenceCleanup
/// for the members it removes, which it applies, and one for the members it adds, whose
/// held places it restores. Its inverse exchanges the two, so the edit that takes a removal
/// back restores the places the removal held. Neither cleanup is part of its start
/// condition.
class UpdateFamilyCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "updateFamily";

    /// The edit from the family as opened to the family as saved, or an empty pointer when
    /// nothing differs, which WardSession::commit takes as nothing to do. Each cleanup holds
    /// the places in document that refer to the members it names.
    static UndoableCommandPtr fromEdit(const Family& opened, const Family& saved, const Document& document);

    UpdateFamilyCommand(const FamilyId& familyId,
                        const FamilyChange& change,
                        const ReferenceCleanup& addedCleanup,
                        const ReferenceCleanup& removedCleanup,
                        const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    CommandPtr inverse() const override;
    QJsonObject toJson() const override;

    /// The whole document when it can change a place outside the family: it removes a
    /// member, or restores a held place. Otherwise the family.
    DocumentChange documentChange() const override;

    /// The address after, when the change sets a non-empty address.
    QList<Address> addressesGiven() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    FamilyId m_familyId;
    FamilyChange m_change;
    ReferenceCleanup m_addedCleanup;
    ReferenceCleanup m_removedCleanup;
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
    CommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    Family m_family;
    ReferenceCleanup m_cleanup;
};

/// A deleted family put back, with its people restored to the places the deletion held that
/// still stand as it left them. Its result holds when the family is there; it never
/// conflicts.
class RestoreFamilyCommand : public Command
{
public:
    static constexpr const char* TypeName = "restoreFamily";

    RestoreFamilyCommand(const Family& family, const ReferenceCleanup& cleanup, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    /// The family's address, when it is not empty.
    QList<Address> addressesGiven() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    Family m_family;
    ReferenceCleanup m_cleanup;
};
