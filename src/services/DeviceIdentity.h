#pragma once

#include "CryptoTypes.h"
#include "DocumentCrypto.h"
#include "Reconcile.h"
#include "RosterDevice.h"
#include "RosterPerson.h"

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
    RosterDevice asRosterDevice() const;

    /// This person, an admin, with only this device.
    RosterPerson asNewPerson() const;
};
