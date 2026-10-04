#include "DeviceIdentity.h"

Fingerprint DeviceIdentity::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey);
}

Reconcile::Self DeviceIdentity::self() const
{
    return Reconcile::Self{sealing, signing.verifyKey};
}

RosterDevice DeviceIdentity::asRosterDevice() const
{
    return RosterDevice::create(deviceName, sealing.publicKey, signing.verifyKey);
}

RosterPerson DeviceIdentity::asNewPerson() const
{
    return RosterPerson::create(member, displayName, asRosterDevice(), RosterPerson::Role::Admin);
}
