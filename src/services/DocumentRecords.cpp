#include "DocumentRecords.h"

bool DocumentRecord::operator==(const DocumentRecord& other) const
{
    return baseVersion == other.baseVersion
        && baseHash == other.baseHash
        && highWaterMark.keyGeneration == other.highWaterMark.keyGeneration
        && highWaterMark.saveCounter == other.highWaterMark.saveCounter
        && pendingChanges == other.pendingChanges;
}
