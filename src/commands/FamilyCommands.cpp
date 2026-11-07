#include "FamilyCommands.h"
#include "Document.h"

#include <QObject>

// ----------------------------------------------------------------------------
// UpdateFamilyCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr UpdateFamilyCommand::fromEdit(const Family& opened, const Family& saved, const Document& document)
{
    std::optional<FamilyChange> change = FamilyChange::between(opened, saved, document);
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
    : UndoableCommand(description)
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
    m_change.applyTo(document, *family);
    return CommandOutcome::Applied;
}

UndoableCommandPtr UpdateFamilyCommand::inverse() const
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
    if (m_change.reachesOtherPlaces())
    {
        return DocumentChange::full();
    }
    return DocumentChange::family().updated(m_familyId);
}

QList<Address> UpdateFamilyCommand::addressesGiven() const
{
    const std::optional<FieldChange<Address>>& address = m_change.address();
    if (!address.has_value() || address->after().isEmpty())
    {
        return {};
    }
    return {address->after()};
}

QStringList UpdateFamilyCommand::skippedOn(const Document& document) const
{
    return m_change.skippedOn(document);
}

CommandPtr UpdateFamilyCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<UpdateFamilyCommand>(FamilyId::fromString(json["familyId"].toString()),
                                                 FamilyChange::fromJson(json["change"].toObject()),
                                                 description);
}

// ----------------------------------------------------------------------------
// DeleteFamilyCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr DeleteFamilyCommand::fromSeen(const Family& family, const Document& document)
{
    QSet<PersonId> people;
    for (const Person& member : family.members())
    {
        people.insert(member.id());
    }
    CommandDescription description(QObject::tr("Delete family: %1").arg(family.displayName()), {});
    return std::make_shared<DeleteFamilyCommand>(family, ReferenceCleanup::of(document, people, {family.id()}),
                                                 description);
}

DeleteFamilyCommand::DeleteFamilyCommand(const Family& family,
                                         const ReferenceCleanup& cleanup,
                                         const CommandDescription& description)
    : UndoableCommand(description)
    , m_family(family)
    , m_cleanup(cleanup)
{
}

CommandOutcome DeleteFamilyCommand::execute(Document& document) const
{
    std::optional<Family> family = document.findFamilyById(m_family.id());
    if (family.has_value() && *family != m_family)
    {
        return CommandOutcome::Conflicted;
    }
    document.removeFamily(m_family.id());
    m_cleanup.apply(document);
    return CommandOutcome::Applied;
}

UndoableCommandPtr DeleteFamilyCommand::inverse() const
{
    return std::make_shared<RestoreFamilyCommand>(m_family, m_cleanup, description().undone());
}

QJsonObject DeleteFamilyCommand::toJson() const
{
    QJsonObject json;
    json["family"] = m_family.toJson();
    json["cleanup"] = m_cleanup.toJson();
    return json;
}

DocumentChange DeleteFamilyCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr DeleteFamilyCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<DeleteFamilyCommand>(Family::fromJson(json["family"].toObject()),
                                                 ReferenceCleanup::fromJson(json["cleanup"].toObject()),
                                                 description);
}

// ----------------------------------------------------------------------------
// RestoreFamilyCommand
// ----------------------------------------------------------------------------

RestoreFamilyCommand::RestoreFamilyCommand(const Family& family,
                                           const ReferenceCleanup& cleanup,
                                           const CommandDescription& description)
    : UndoableCommand(description)
    , m_family(family)
    , m_cleanup(cleanup)
{
}

CommandOutcome RestoreFamilyCommand::execute(Document& document) const
{
    if (!document.findFamilyById(m_family.id()).has_value())
    {
        document.setFamily(m_family);
    }
    m_cleanup.restore(document);
    return CommandOutcome::Applied;
}

UndoableCommandPtr RestoreFamilyCommand::inverse() const
{
    return std::make_shared<DeleteFamilyCommand>(m_family, m_cleanup, description().undone());
}

QJsonObject RestoreFamilyCommand::toJson() const
{
    QJsonObject json;
    json["family"] = m_family.toJson();
    json["cleanup"] = m_cleanup.toJson();
    return json;
}

DocumentChange RestoreFamilyCommand::documentChange() const
{
    return DocumentChange::full();
}

QList<Address> RestoreFamilyCommand::addressesGiven() const
{
    if (m_family.address().isEmpty())
    {
        return {};
    }
    return {m_family.address()};
}

QStringList RestoreFamilyCommand::skippedOn(const Document& document) const
{
    return m_cleanup.skippedOn(document);
}

CommandPtr RestoreFamilyCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<RestoreFamilyCommand>(Family::fromJson(json["family"].toObject()),
                                                  ReferenceCleanup::fromJson(json["cleanup"].toObject()),
                                                  description);
}
