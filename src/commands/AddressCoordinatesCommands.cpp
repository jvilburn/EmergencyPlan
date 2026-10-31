#include "AddressCoordinatesCommands.h"
#include "Document.h"

#include <QObject>

namespace
{

/// The entry the table holds for the address text, or none.
std::optional<Coordinates> entryFor(const Document& document, const QString& address)
{
    auto it = document.addressCoordinates().constFind(address);
    if (it != document.addressCoordinates().constEnd())
    {
        return *it;
    }
    return std::nullopt;
}

/// The fields both commands hold, which are the same in both directions.
QJsonObject entryChangeToJson(const QString& address, const std::optional<Coordinates>& before, const Coordinates& typed)
{
    QJsonObject json;
    json["address"] = address;
    if (before.has_value())
    {
        json["before"] = before->toJson();
    }
    json["typed"] = typed.toJson();
    return json;
}

std::optional<Coordinates> beforeFromJson(const QJsonObject& json)
{
    if (!json.contains("before"))
    {
        return std::nullopt;
    }
    return Coordinates::fromJson(json["before"].toObject());
}

}  // namespace

UndoableCommandPtr SetAddressCoordinatesCommand::fromTyped(const Document& document,
                                                           const QString& address,
                                                           const Coordinates& typed)
{
    std::optional<Coordinates> before = entryFor(document, address);
    if (before == typed)
    {
        return nullptr;
    }
    QString line = QObject::tr("latitude %1, longitude %2")
                       .arg(QString::number(typed.latitude(), 'f', 6), QString::number(typed.longitude(), 'f', 6));
    CommandDescription description(QObject::tr("Map position for %1").arg(address), {line});
    return std::make_shared<SetAddressCoordinatesCommand>(address, before, typed, description);
}

SetAddressCoordinatesCommand::SetAddressCoordinatesCommand(const QString& address,
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
    document.setAddressCoordinates(m_address, m_typed);
    return CommandOutcome::Applied;
}

UndoableCommandPtr SetAddressCoordinatesCommand::inverse() const
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

CommandPtr SetAddressCoordinatesCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<SetAddressCoordinatesCommand>(json["address"].toString(),
                                                          beforeFromJson(json),
                                                          Coordinates::fromJson(json["typed"].toObject()),
                                                          description);
}

RevertAddressCoordinatesCommand::RevertAddressCoordinatesCommand(const QString& address,
                                                                 const std::optional<Coordinates>& before,
                                                                 const Coordinates& typed,
                                                                 const CommandDescription& description)
    : UndoableCommand(description)
    , m_address(address)
    , m_before(before)
    , m_typed(typed)
{
}

CommandOutcome RevertAddressCoordinatesCommand::execute(Document& document) const
{
    std::optional<Coordinates> current = entryFor(document, m_address);
    if (current == m_before)
    {
        return CommandOutcome::Applied;
    }
    if (current != m_typed)
    {
        return CommandOutcome::Conflicted;
    }
    document.setAddressCoordinates(m_address, m_before);
    return CommandOutcome::Applied;
}

UndoableCommandPtr RevertAddressCoordinatesCommand::inverse() const
{
    return std::make_shared<SetAddressCoordinatesCommand>(m_address, m_before, m_typed, description().undone());
}

QJsonObject RevertAddressCoordinatesCommand::toJson() const
{
    return entryChangeToJson(m_address, m_before, m_typed);
}

DocumentChange RevertAddressCoordinatesCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr RevertAddressCoordinatesCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<RevertAddressCoordinatesCommand>(json["address"].toString(),
                                                             beforeFromJson(json),
                                                             Coordinates::fromJson(json["typed"].toObject()),
                                                             description);
}
