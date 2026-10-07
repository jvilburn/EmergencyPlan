#include "FileOpenRouterTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "DocumentLibrary.h"
#include "FileOpenRouter.h"
#include "SessionTestSupport.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace SessionTestSupport;

namespace
{

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

/// A document whose roster is only this device's person.
Document documentFor(const DeviceIdentity& identity)
{
    AccessRoster roster;
    roster.addPerson(identity.asNewPerson());
    Document document = Document::empty();
    document.setAccessRoster(roster);
    return document;
}

/// A ward's own file under documentId, sealed by identity, its header claiming
/// formatVersion.
QByteArray wardFile(const QString& documentId, const DeviceIdentity& identity, int formatVersion)
{
    DocumentHeader first;
    first.documentId = documentId;
    first.saveCounter = 0;
    DocumentFile::Sealed sealed =
        DocumentFile::seal(documentFor(identity), first, DocumentCrypto::generateDataKey(), identity).value();
    sealed.header.formatVersion = formatVersion;
    return DocumentFile::encode(sealed);
}

} // namespace

void FileOpenRouterTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void FileOpenRouterTest::eachFileTakesItsRoute()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const DeviceIdentity alice = newIdentity("Alice", "Laptop");
    const std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library(*settings, folder.filePath("wards"), alice);

    const QString wardId = DocumentLibrary::newDocumentId();
    library.add(wardId);

    // A ward in the library, its file somewhere other than the library's own.
    const QString ward = folder.filePath("ward on a stick.wardplanning");
    QVERIFY(writeFile(ward, wardFile(wardId, alice, DocumentHeader::CurrentFormatVersion)));

    // A copy written from that ward.
    const QString copy = folder.filePath("copy.wardplanning");
    QVERIFY(library.exportCopy(wardId, documentFor(alice), copy, nullptr));

    // A ward this device does not have.
    const QString strangerId = DocumentLibrary::newDocumentId();
    const QString stranger = folder.filePath("stranger.wardplanning");
    QVERIFY(writeFile(stranger, wardFile(strangerId, newIdentity("Bob", "Phone"), DocumentHeader::CurrentFormatVersion)));

    const QString later = folder.filePath("later.wardplanning");
    QVERIFY(writeFile(later, wardFile(DocumentLibrary::newDocumentId(), alice, DocumentHeader::CurrentFormatVersion + 1)));

    const QString notOurs = folder.filePath("notes.wardplanning");
    QVERIFY(writeFile(notOurs, "shopping list"));

    const QString missing = folder.filePath("missing.wardplanning");

    FileOpenRouter::Route route = FileOpenRouter::route(ward, library);
    QCOMPARE(route.action, FileOpenRouter::Action::OpenWard);
    QCOMPARE(route.documentId, wardId);

    route = FileOpenRouter::route(copy, library);
    QCOMPARE(route.action, FileOpenRouter::Action::OpenCopy);

    route = FileOpenRouter::route(stranger, library);
    QCOMPARE(route.action, FileOpenRouter::Action::RequestAccess);
    QCOMPARE(route.documentId, strangerId);

    QCOMPARE(FileOpenRouter::route(later, library).action, FileOpenRouter::Action::NeedsNewerVersion);
    QCOMPARE(FileOpenRouter::route(notOurs, library).action, FileOpenRouter::Action::NotAWardPlanningFile);
    QCOMPARE(FileOpenRouter::route(missing, library).action, FileOpenRouter::Action::Unreadable);

    // Once the stranger's ward is in the library, its file opens it.
    library.add(strangerId);
    route = FileOpenRouter::route(stranger, library);
    QCOMPARE(route.action, FileOpenRouter::Action::OpenWard);
    QCOMPARE(route.documentId, strangerId);
}
