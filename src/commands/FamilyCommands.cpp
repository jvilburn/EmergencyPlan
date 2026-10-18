#include "FamilyCommands.h"
#include "Document.h"

#include <QObject>

CommandPtr UpdateFamilyCommand::fromEdit(const Family& opened, const Family& saved)
{
    std::optional<FamilyChange> change = FamilyChange::between(opened, saved);
    if (!change.has_value())
    {
        return nullptr;
    }
    CommandDescription description(QObject::tr("%1 family").arg(opened.surname()), change->describe(opened));
    return std::make_shared<UpdateFamilyCommand>(opened.id(), *change, description);
}

UpdateFamilyCommand::UpdateFamilyCommand(const FamilyId& familyId,
                                         const FamilyChange& change,
                                         const CommandDescription& description)
    : Command(description)
    , m_familyId(familyId)
    , m_change(change)
{
}

CommandOutcome UpdateFamilyCommand::execute(Document& document) const
{
    std::optional<Family> family = document.findFamilyById(m_familyId);
    if (!family.has_value() || !m_change.holds(*family))
    {
        return CommandOutcome::Conflicted;
    }
    document.updateFamily(m_change.appliedTo(*family));
    return CommandOutcome::Applied;
}

CommandPtr UpdateFamilyCommand::inverse() const
{
    return std::make_shared<UpdateFamilyCommand>(m_familyId, m_change.swapped(), description().undone());
}

QJsonObject UpdateFamilyCommand::toJson() const
{
    QJsonObject json;
    json["familyId"] = m_familyId.toString();
    json["change"] = m_change.toJson();
    return json;
}

DocumentChange UpdateFamilyCommand::documentChange() const
{
    return DocumentChange::family().updated(m_familyId);
}

CommandPtr UpdateFamilyCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<UpdateFamilyCommand>(FamilyId::fromString(json["familyId"].toString()),
                                                 FamilyChange::fromJson(json["change"].toObject()),
                                                 description);
}
