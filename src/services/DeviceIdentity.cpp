#include "DeviceIdentity.h"

Fingerprint DeviceIdentity::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey);
}

Reconcile::Self DeviceIdentity::self() const
{
    return Reconcile::Self{sealing, signing.verifyKey};
}

NamedDevice DeviceIdentity::device() const
{
    return NamedDevice{deviceName, keys()};
}
