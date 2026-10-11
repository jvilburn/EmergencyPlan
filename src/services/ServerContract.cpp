#include "ServerContract.h"

namespace ServerContract
{

bool Device::operator==(const Device& other) const
{
    return fingerprint == other.fingerprint
        && keys == other.keys
        && name == other.name
        && linkedBy == other.linkedBy
        && linkedAt == other.linkedAt;
}

bool Person::operator==(const Person& other) const
{
    return member == other.member && displayName == other.displayName && devices == other.devices;
}

bool LostAccess::operator==(const LostAccess& other) const
{
    return how == other.how && byWhom == other.byWhom && when == other.when;
}

} // namespace ServerContract
