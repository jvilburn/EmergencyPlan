#pragma once

#include "DocumentCrypto.h"
#include "SealingSet.h"

#include <QByteArray>
#include <QString>

#include <optional>

/// An invite's keypairs, both from its seed through their own hash domains.
struct InviteKeys
{
    Keypair sealing;
    SigningKeypair signing;

    /// The halves the roster lists and the header seals to.
    DeviceKeys publicKeys() const { return DeviceKeys{sealing.publicKey, signing.verifyKey}; }
    Fingerprint fingerprint() const;
};

/// An invite: the ward, and the seed the invite's keypairs are made from. Whoever
/// holds it can read the ward and join it once.
struct InviteLink
{
    static constexpr int SeedSize = 32;

    QString documentId;
    QByteArray seed;

    InviteKeys keys() const;

    /// The roster entry this link joins with.
    Invite invite(Invite::For purpose, const MemberId& maker, Role role, const QString& label) const;

    /// `wardplanning:join/<document id>/<seed, base64url>`.
    QString toString() const;

    /// What the person copies and sends: who invites them to which ward, where to
    /// get the app, and the link.
    QString message(const QString& inviterName, const QString& wardName) const;

    /// The link an argument is; nullopt for anything else, a malformed one included.
    static std::optional<InviteLink> parse(const QString& argument);
};
