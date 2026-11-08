#pragma once

#include "Document.h"
#include "Identity.h"
#include "WardPayload.h"

/// Values in every field, for tests that check the payload whole.
class EveryField
{
public:
    /// A ward with a value in every field of every model: the test ward's people in every
    /// kind of place, with Bob holding every Person field and the Smith family every Family
    /// field; an address coordinates entry; ministering groups and districts in both
    /// organizations with every role filled; ward and stake records with every field; both
    /// PDF dates; and the Storm emergency holding an attempt with every field, an attempt
    /// with only an outcome, a task assigned to a person, a task assigned to a team whose
    /// assignment is notified and which is resolved, and an added task category.
    static Document ward();

    /// A local file's payload with a value in every field of every part: ward() with this
    /// access list - Alice Smith, an admin, on her Laptop (laptop) and Pixel 8 (phone); Carol
    /// Jones, a member, on her Desktop; a member invite Alice made for Bob; a device invite
    /// Carol made; and one former key, so its recipients in order are laptop, phone,
    /// Desktop, Bob's invite and Carol's device invite. Landed marks of 3 for laptop and 5
    /// for phone, and a value in every field of this device's state, whose journal holds the
    /// first sample of every registered command type in registry order.
    static WardPayload payload(const Identity& laptop, const Identity& phone);
};
