#include "DeviceIdentity.h"

Fingerprint DeviceIdentity::fingerprint() const
{
    return DocumentCrypto::fingerprintOf(sealing.publicKey, signing.verifyKey);
}

Reconcile::Self DeviceIdentity::self() const
{
    return Reconcile::Self{sealing, signing.verifyKey};
}

ServerContract::Registration DeviceIdentity::registration() const
{
    return ServerContract::Registration{member, displayName, deviceName, sealing.publicKey};
}
