#include "FileOpenRouter.h"

#include "DeviceIdentity.h"
#include "DocumentFile.h"
#include "DocumentLibrary.h"

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

} // namespace FileOpenRouter
