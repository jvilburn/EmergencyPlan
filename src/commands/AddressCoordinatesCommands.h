#pragma once

#include "Command.h"
#include "Coordinates.h"

#include <optional>

/// Coordinates a user typed, set on the entry for their address in the table of address
/// coordinates. Sets the entry whatever it holds, so it never conflicts; its result already
/// holds when the entry equals the typed coordinates.
class SetAddressCoordinatesCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "setAddressCoordinates";

    /// The command that sets typed on the entry for address, with the entry as the document
    /// holds it now as before, or an empty pointer when the entry already equals typed,
    /// which WardSession::commit takes as nothing to do.
    static UndoableCommandPtr fromTyped(const Document& document, const QString& address, const Coordinates& typed);

    SetAddressCoordinatesCommand(const QString& address,
                                 const std::optional<Coordinates>& before,
                                 const Coordinates& typed,
                                 const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QString m_address;
    std::optional<Coordinates> m_before;
    Coordinates m_typed;
};

/// The inverse of SetAddressCoordinatesCommand: puts back what the entry held before, when
/// it still holds the typed coordinates. Its result already holds when the entry holds
/// before, and any other entry makes it conflict.
class RevertAddressCoordinatesCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "revertAddressCoordinates";

    RevertAddressCoordinatesCommand(const QString& address,
                                    const std::optional<Coordinates>& before,
                                    const Coordinates& typed,
                                    const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QString m_address;
    std::optional<Coordinates> m_before;
    Coordinates m_typed;
};
