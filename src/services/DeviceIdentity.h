#pragma once

#include "CryptoTypes.h"
#include "DocumentCrypto.h"
#include "Reconcile.h"
#include "SealingSet.h"

#include <QString>

/// This device, as the core sees it: a value, fixed once a person exists.
/// IdentityService produces it.
struct DeviceIdentity
{
    Keypair sealing;
    SigningKeypair signing;
    MemberId member;
    QString displayName;
    QString deviceName;

    Fingerprint fingerprint() const;
    Reconcile::Self self() const;
    DeviceKeys keys() const { return DeviceKeys{sealing.publicKey, signing.verifyKey}; }

    /// This device as a roster lists it.
    NamedDevice device() const;
};
