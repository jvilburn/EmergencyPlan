#pragma once

#include "AccessList.h"
#include "DeviceState.h"
#include "Identity.h"

/// Values in every field, for tests that check a part of the payload whole.
class EveryField
{
public:
    /// Alice Smith, an admin, on her Laptop (laptop) and Pixel 8 (phone); Carol Jones, a
    /// member, on her Desktop; a member invite Alice made for Bob; a device invite Carol made;
    /// and one former key. Its recipients, in order: laptop, phone, Desktop, Bob's invite,
    /// Carol's device invite.
    static AccessList access(const Identity& laptop, const Identity& phone);

    /// A value in every field of this device's state. The journal holds the first sample of
    /// every registered command type, in registry order.
    static DeviceState deviceState(const AccessList& access);
};
