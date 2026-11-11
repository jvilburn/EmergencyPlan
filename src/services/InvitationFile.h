#pragma once

#include <QByteArray>
#include <QString>

enum class InvitationReadStatus
{
    Read,
    Newer,           ///< its format is above every version this app reads
    NotAnInvitation  ///< it is not an invitation this app reads; problem says where
};

/// An invitation file split into its parts.
struct InvitationRead
{
    InvitationReadStatus status;
    QString problem;     ///< when NotAnInvitation
    QByteArray seed;     ///< when Read
    QByteArray version;  ///< when Read: the version, in a ward file's framing
};

/// A .wardinvite file: one line of compact JSON holding its own format and the invite's seed,
/// one newline, then the version that opened the invite, in a ward file's framing, with no
/// device part. The version's header names the ward.
class InvitationFile
{
public:
    /// The format this app writes, and the only one it reads.
    static constexpr int Format = 1;

    /// The bytes of an invitation for the invite seed gives, carrying version.
    static QByteArray write(const QByteArray& seed, const QByteArray& version);

    /// bytes split at the first newline, the line read strictly: a format above Format is
    /// Newer, and anything that is not this format's line is NotAnInvitation.
    static InvitationRead read(const QByteArray& bytes);

    /// The name an invitation to the ward named wardName is saved under, from how the
    /// interface names the invite: Tanglewood Ward - for Bob.wardinvite.
    static QString fileName(const QString& wardName, const QString& inviteDescription);
};
