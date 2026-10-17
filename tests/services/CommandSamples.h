#pragma once

#include "Command.h"
#include "Document.h"

#include <QString>
#include <vector>

/// Which outcome a sample's document gives its command.
enum class SampleCase
{
    StartHolds,   ///< applies and changes the document
    ResultHolds,  ///< applies and leaves the document as it was
    Conflicts     ///< conflicts and leaves the document as it was
};

struct CommandSample
{
    SampleCase sampleCase;
    Document document;
    CommandPtr command;
};

/// Samples of each registered command type, one per outcome the type can give.
class CommandSamples
{
public:
    /// The samples for the type with that name; empty for a type with none.
    static std::vector<CommandSample> forType(const QString& type);
};
