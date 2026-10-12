#include "FileOpenRouter.h"

#include "DeviceIdentity.h"
#include "DocumentFile.h"
#include "DocumentConstants.h"
#include "DocumentLibrary.h"
#include "HexCodec.h"

namespace FileOpenRouter
{

Route route(const QString& path, const DocumentLibrary& library, const DeviceIdentity& identity)
{
    const std::optional<QByteArray> bytes = DocumentFile::readHeader(path);
    if (!bytes.has_value())
    {
        return Route{Action::Unreadable, QString()};
    }

    DocumentFile::Sealed sealed;
    switch (DocumentFile::decode(bytes.value(), sealed))
    {
    case DocumentFile::Read::Ok:
        break;
    case DocumentFile::Read::NeedsNewerVersion:
        return Route{Action::NeedsNewerVersion, QString()};
    case DocumentFile::Read::Malformed:
        return Route{Action::NotAWardPlanningFile, QString()};
    }

    const DocumentHeader& header = sealed.header;
    if (library.contains(header.documentId))
    {
        return Route{Action::OpenWard, header.documentId};
    }
    if (!header.originDocumentId.isEmpty())
    {
        return Route{Action::OpenCopy, QString()};
    }
    if (header.entryFor(identity.fingerprint()).has_value())
    {
        return Route{Action::AddWard, header.documentId};
    }
    return Route{Action::RequestAccess, header.documentId};
}

QString inviteLink(const QString& documentId)
{
    return QString::fromLatin1(DocumentConstants::InvitePrefix) + documentId;
}

QString invitedWard(const QString& argument)
{
    const QString prefix = QString::fromLatin1(DocumentConstants::InvitePrefix);
    if (!argument.startsWith(prefix, Qt::CaseInsensitive))
    {
        return QString();
    }

    // A browser may add a trailing slash; anything else is not an id.
    QString documentId = argument.mid(prefix.size());
    if (documentId.endsWith('/'))
    {
        documentId.chop(1);
    }
    return HexCodec::isCanonical(documentId, DocumentHeader::DocumentIdLength) ? documentId : QString();
}

Route routeInvite(const QString& documentId, const DocumentLibrary& library)
{
    return Route{library.contains(documentId) ? Action::OpenWard : Action::RequestAccess, documentId};
}

} // namespace FileOpenRouter
