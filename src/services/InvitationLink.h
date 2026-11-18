#pragma once

#include "Id.h"

#include <QByteArray>
#include <QString>
#include <optional>

/// What an invitation link carries: which ward, and the invite's seed.
struct InvitationCode
{
    DocumentId document;
    QByteArray seed;
};

/// An invitation as a link, for a ward shared through the server: the server's join page,
/// then, after the #, the code, which is the ward's document id, a dot and the invite's seed,
/// both base64url, then a dot and the ward's name. A browser keeps what follows the # to
/// itself, so the seed reaches no server. The code is what the installer carries in its file
/// name and hands to the app; the name is for the join page to show.
class InvitationLink
{
public:
    /// The code for the invite seed gives, to the ward document: <document>.<seed>.
    static QString code(const DocumentId& document, const QByteArray& seed);

    /// What code holds, when it is a document id of 16 bytes, a dot and a seed of 32 bytes,
    /// each in base64url; otherwise none.
    static std::optional<InvitationCode> parse(const QString& code);

    /// The link that carries code, and wardName percent-encoded, to the join page of the
    /// server at serverAddress.
    static QString link(const QString& serverAddress, const QString& code, const QString& wardName);
};
