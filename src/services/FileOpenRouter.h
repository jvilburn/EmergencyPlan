#pragma once

#include "DocumentCrypto.h"

#include <QByteArray>
#include <QString>

#include <optional>

class DocumentLibrary;
struct DeviceIdentity;

/// Where a .wardplanning file or an invite link goes - a double-click, a link
/// clicked, an argument given at startup, or one a later launch handed over. Among
/// .wardplanning files, the header decides where one goes, not its name or folder.
namespace FileOpenRouter
{

enum class Action
{
    OpenWard,               ///< a ward in the library: open it by id
    OpenCopy,               ///< an exported copy: open it read-only
    AddWard,                ///< a ward written for this device but not in its library
    NotForThisDevice,       ///< a ward not written for this device: it takes an invite
    NeedsNewerVersion,      ///< written by a later version of Ward Planning
    NotAWardPlanningFile,   ///< no readable Ward Planning header
    Unreadable              ///< missing, or could not be read
};

struct Route
{
    Action action;
    QString documentId;     ///< the ward, for every action that names one
};

Route route(const QString& path, const DocumentLibrary& library, const DeviceIdentity& identity);

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

    /// `wardplanning:join/<document id>/<seed, base64url>`.
    QString toString() const;

    /// The link an argument is; nullopt for anything else, a malformed one included.
    static std::optional<InviteLink> parse(const QString& argument);
};

/// What the person copies and sends: who invites them to which ward, where to get the
/// app, and the link.
QString inviteMessage(const QString& inviterName, const QString& wardName, const InviteLink& link);

} // namespace FileOpenRouter
