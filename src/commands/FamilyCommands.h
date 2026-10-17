#pragma once

#include "Command.h"
#include "FamilyChange.h"

#include <memory>

/// An edit to one family, built from the difference between what the edit opened with and
/// what it saved. Holds the before and after of the changed fields only.
class UpdateFamilyCommand : public Command
{
public:
    static constexpr const char* TypeName = "updateFamily";

    /// The edit from the family as opened to the family as saved, or an empty pointer when
    /// nothing differs.
    static std::unique_ptr<UpdateFamilyCommand> fromEdit(const Family& opened, const Family& saved);

    UpdateFamilyCommand(const FamilyId& familyId, const FamilyChange& change, const Description& description);

    QString type() const override { return TypeName; }
    Outcome execute(Document& document) const override;
    CommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const Description& description);

private:
    FamilyId m_familyId;
    FamilyChange m_change;
};
