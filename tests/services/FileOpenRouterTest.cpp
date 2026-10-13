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

/// A document sealed only to this device.
Document documentFor(const DeviceIdentity& identity)
{
    Document document = Document::empty();
    document.setSealingSet(SealingSet::of(identity.member, identity.displayName, identity.device()));
    return document;
}

/// A file under documentId, sealed to identity's device, with the origin and format
/// version given.
QByteArray wardFile(const QString& documentId,
                    const DeviceIdentity& identity,
                    const QString& originDocumentId,
                    int formatVersion)
{
    DocumentHeader first;
    first.documentId = documentId;
    first.originDocumentId = originDocumentId;
    first.saveCounter = 0;
    DocumentFile::Sealed sealed =
        DocumentFile::seal(documentFor(identity), first, DocumentCrypto::generateDataKey()).value();
    sealed.header.formatVersion = formatVersion;
    return DocumentFile::encode(sealed);
}

} // namespace

void FileOpenRouterTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void FileOpenRouterTest::eachFileTakesItsRoute_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<FileOpenRouter::Action>("action");
    QTest::addColumn<bool>("namesTheWard");

    QTest::newRow("a ward in the library") << "ward" << FileOpenRouter::Action::OpenWard << true;
    QTest::newRow("a ward in the library, with an origin")
        << "ward with origin" << FileOpenRouter::Action::OpenWard << true;
    QTest::newRow("a copy") << "copy" << FileOpenRouter::Action::OpenCopy << false;
    QTest::newRow("written for this device") << "this device" << FileOpenRouter::Action::AddWard << true;
    QTest::newRow("another person's ward") << "stranger" << FileOpenRouter::Action::NotForThisDevice << true;
    QTest::newRow("a later format") << "later" << FileOpenRouter::Action::NeedsNewerVersion << false;
    QTest::newRow("not a Ward Planning file") << "not ours" << FileOpenRouter::Action::NotAWardPlanningFile << false;
    QTest::newRow("a first line longer than any header")
        << "endless" << FileOpenRouter::Action::NotAWardPlanningFile << false;
    QTest::newRow("missing") << "missing" << FileOpenRouter::Action::Unreadable << false;
}

void FileOpenRouterTest::eachFileTakesItsRoute()
{
    QFETCH(QString, file);
    QFETCH(FileOpenRouter::Action, action);
    QFETCH(bool, namesTheWard);

    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const DeviceIdentity laptop = newIdentity("Alice", "Laptop");
    const std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library(*settings, folder.filePath("wards"), laptop);

    const QString wardId = DocumentLibrary::newDocumentId();
    library.add(wardId);

    // Each file is somewhere other than the library's own folder.
    const QString path = folder.filePath("opened.wardplanning");
    QString documentId = DocumentLibrary::newDocumentId();
    const int current = DocumentHeader::CurrentFormatVersion;
    if (file == "ward")
    {
        documentId = wardId;
        QVERIFY(writeFile(path, wardFile(wardId, laptop, QString(), current)));
    }
    else if (file == "ward with origin")
    {
        documentId = wardId;
        QVERIFY(writeFile(path, wardFile(wardId, laptop, DocumentLibrary::newDocumentId(), current)));
    }
    else if (file == "copy")
    {
        QVERIFY(library.exportCopy(wardId, documentFor(laptop), path, nullptr));
    }
    else if (file == "this device")
    {
        QVERIFY(writeFile(path, wardFile(documentId, laptop, QString(), current)));
    }
    else if (file == "stranger")
    {
        QVERIFY(writeFile(path, wardFile(documentId, newIdentity("Bob", "Phone"), QString(), current)));
    }
    else if (file == "later")
    {
        QVERIFY(writeFile(path, wardFile(documentId, laptop, QString(), current + 1)));
    }
    else if (file == "not ours")
    {
        QVERIFY(writeFile(path, "shopping list\nand more"));
    }
    else if (file == "endless")
    {
        QVERIFY(writeFile(path, QByteArray(DocumentFile::MaxHeaderBytes + 1, '{')));
    }

    const FileOpenRouter::Route route = FileOpenRouter::route(path, library, laptop);
    QCOMPARE(route.action, action);
    QCOMPARE(route.documentId, namesTheWard ? documentId : QString());
}
