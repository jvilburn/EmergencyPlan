#pragma once

#include "CryptoTypes.h"
#include "DocumentCrypto.h"
#include "Reconcile.h"
#include "ServerContract.h"

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

    /// What ServerConnection registers after every sign-in.
    ServerContract::Registration registration() const;
};
