#pragma once

#include <QString>
#include <memory>

class Document;

class Command
{
public:
    virtual ~Command() = default;

    // Execute command, modifying document in place
    virtual void execute(Document& document) = 0;

    // Undo command, modifying document in place
    virtual void undo(Document& document) = 0;

    // Human-readable description for undo/redo menu items
    virtual QString description() const = 0;

    // Returns ID of family whose address was changed by this command.
    // Empty string if no address changed. Used to trigger background geocoding.
    virtual QString familyWithChangedAddress() const { return {}; }
};

using CommandPtr = std::unique_ptr<Command>;
