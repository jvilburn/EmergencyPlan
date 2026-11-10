#include "AddressCoordinatesCommands.h"
#include "Document.h"

#include <QJsonArray>
#include <QObject>

namespace
{

/// The fields both commands hold, which are the same in both directions.
QJsonObject entryChangeToJson(const Address& address, const std::optional<Coordinates>& before, const Coordinates& typed)
{
    QJsonObject json;
    json["address"] = QJsonArray::fromStringList(address.lines());
    if (before.has_value())
    {
        json["before"] = before->toJson();
    }
    json["typed"] = typed.toJson();
    return json;
}

/// The fields entryChangeToJson wrote, which both commands read the same way.
struct EntryChange
{
    Address address;
    std::optional<Coordinates> before;
    Coordinates typed;
};

EntryChange entryChangeFromJson(JsonReader json)
{
    Address address = Address::fromLines(json.strings("address"));
    std::optional<Coordinates> before;
    std::optional<JsonReader> beforeJson = json.optionalObject("before");
    if (beforeJson.has_value())
    {
        before = Coordinates::fromJson(*beforeJson);
    }
    Coordinates typed = Coordinates::fromJson(json.object("typed"));
    json.finish();
    return EntryChange{address, before, typed};
}

}  // namespace

UndoableCommandPtr SetAddressCoordinatesCommand::fromTyped(const Document& document,
                                                           const Address& address,
                                                           const Coordinates& typed)
{
    std::optional<Coordinates> before = document.coordinatesFor(address);
    if (before == typed)
    {
        return nullptr;
    }
    QString line = QObject::tr("latitude %1, longitude %2")
                       .arg(QString::number(typed.latitude(), 'f', 6), QString::number(typed.longitude(), 'f', 6));
    CommandDescription description(QObject::tr("Map position for %1").arg(address.full()), {line});
    return std::make_shared<SetAddressCoordinatesCommand>(address, before, typed, description);
}

SetAddressCoordinatesCommand::SetAddressCoordinatesCommand(const Address& address,
                                                           const std::optional<Coordinates>& before,
                                                           const Coordinates& typed,
                                                           const CommandDescription& description)
    : UndoableCommand(description)
    , m_address(address)
    , m_before(before)
    , m_typed(typed)
{
}

CommandOutcome SetAddressCoordinatesCommand::execute(Document& document) const
{
    document.setAddressCoordinates(m_address.full(), m_typed);
    return CommandOutcome::Applied;
}

CommandPtr SetAddressCoordinatesCommand::inverse() const
{
    return std::make_shared<RevertAddressCoordinatesCommand>(m_address, m_before, m_typed, description().undone());
}

QJsonObject SetAddressCoordinatesCommand::toJson() const
{
    return entryChangeToJson(m_address, m_before, m_typed);
}

DocumentChange SetAddressCoordinatesCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr SetAddressCoordinatesCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    EntryChange change = entryChangeFromJson(json);
    return std::make_shared<SetAddressCoordinatesCommand>(change.address, change.before, change.typed, description);
}

RevertAddressCoordinatesCommand::RevertAddressCoordinatesCommand(const Address& address,
                                                                 const std::optional<Coordinates>& before,
                                                                 const Coordinates& typed,
                                                                 const CommandDescription& description)
    : Command(description)
    , m_address(address)
    , m_before(before)
    , m_typed(typed)
{
}

CommandOutcome RevertAddressCoordinatesCommand::execute(Document& document) const
{
    std::optional<Coordinates> current = document.coordinatesFor(m_address);
    if (current == m_before)
    {
        return CommandOutcome::Applied;
    }
    if (current != m_typed)
    {
        return CommandOutcome::Conflicted;
    }
    document.setAddressCoordinates(m_address.full(), m_before);
    return CommandOutcome::Applied;
}

QJsonObject RevertAddressCoordinatesCommand::toJson() const
{
    return entryChangeToJson(m_address, m_before, m_typed);
}

DocumentChange RevertAddressCoordinatesCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr RevertAddressCoordinatesCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    EntryChange change = entryChangeFromJson(json);
    return std::make_shared<RevertAddressCoordinatesCommand>(change.address, change.before, change.typed, description);
}
