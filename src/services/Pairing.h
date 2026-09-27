#pragma once

#include "CryptoTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>

/// Two devices authenticating each other's public keys by comparing a six-digit
/// number, with the server relaying. Used for linking a device and for in-person
/// invites. It never carries the data key.
///
/// The order is what makes it safe, and each side binds what it read:
///
/// 1. The initiator posts commit(initiator, nonce).
/// 2. The responder fetches that commitment and keeps it, and only then posts its
///    own keys.
/// 3. The initiator fetches the responder's keys and keeps them, and only then
///    reveals its keys and nonce.
/// 4. The responder checks revealMatches against the commitment it kept; the
///    initiator computes its number from the responder keys it kept.
/// 5. Both display comparisonNumber from those kept values.
///
/// A relay substituting keys must commit before it sees the responder's keys, and
/// learns the nonce only after choosing them, so it has one chance in a million per
/// attempt. Re-reading either value afterwards is what would let it search for a
/// commitment and nonce that reproduce the number, so neither side ever does.
namespace Pairing
{

/// 32 random bytes; the initiator's secret until it reveals.
QByteArray newNonce();

/// BLAKE2b over the initiator's keys and nonce, under its own personalisation.
QByteArray commit(const DeviceKeys& initiator, const QByteArray& nonce);

/// Constant-time. False for any commitment that is not exactly commit(initiator, nonce).
bool revealMatches(const QByteArray& commitment,
                   const DeviceKeys& initiator,
                   const QByteArray& nonce);

/// Six digits in two groups of three, "482 193", from both devices' keys and the
/// nonce, under a personalisation distinct from the commitment's.
QString comparisonNumber(const DeviceKeys& initiator,
                         const DeviceKeys& responder,
                         const QByteArray& nonce);

/// Eight digits someone can type. A lookup handle issued by the server, not a secret.
QString newPairingCode();

/// What the responder tells the initiator once the numbers match. It carries the
/// identity, so nothing about who the new device belongs to comes from the server.
struct Handover
{
    MemberId member;
    QString displayName;
    QList<QString> documentIds;     ///< the wards the initiator is being added to
    QByteArray signature;
};

/// Signed under SignatureTag::Pairing over the pairing's nonce as well as the
/// contents, so a handover belongs to this pairing only.
QByteArray signHandover(const Handover& handover,
                        const QByteArray& nonce,
                        const SigningKey& responderKey);

bool handoverVerifies(const Handover& handover,
                      const QByteArray& nonce,
                      const VerifyKey& responderVerifyKey);

} // namespace Pairing
