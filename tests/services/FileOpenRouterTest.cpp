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
    QTest::newRow("written for another of this person's devices")
        << "other device" << FileOpenRouter::Action::NotForThisDevice << true;
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
    else if (file == "other device")
    {
        QVERIFY(writeFile(path, wardFile(documentId, newIdentity("Alice", "Phone"), QString(), current)));
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

namespace
{

/// Bytes 0, 1, ... 31: a seed anyone can reproduce.
QByteArray countingSeed()
{
    QByteArray seed;
    for (int i = 0; i < FileOpenRouter::InviteLink::SeedSize; ++i)
    {
        seed.append(char(i));
    }
    return seed;
}

const QString VectorId = "00112233445566778899aabbccddeeff";
const QString VectorLink = "wardplanning:join/00112233445566778899aabbccddeeff/AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8";

} // namespace

void FileOpenRouterTest::anInviteLinkRoundTripsThroughItsText()
{
    const FileOpenRouter::InviteLink link{VectorId, countingSeed()};
    QCOMPARE(link.toString(), VectorLink);
    QCOMPARE(link.toString().size(), 94);

    const std::optional<FileOpenRouter::InviteLink> parsed = FileOpenRouter::InviteLink::parse(VectorLink);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->documentId, VectorId);
    QCOMPARE(parsed->seed, countingSeed());

    // A browser's trailing slash, and a scheme in capitals, are the same link.
    QCOMPARE(FileOpenRouter::InviteLink::parse(VectorLink + "/")->seed, countingSeed());
    QCOMPARE(FileOpenRouter::InviteLink::parse("WardPlanning:Join/" + VectorLink.mid(18))->documentId, VectorId);

    // Any seed survives the trip, including those whose base64 uses '-' and '_'.
    for (int i = 0; i < 20; ++i)
    {
        const FileOpenRouter::InviteLink random{DocumentLibrary::newDocumentId(),
                                                DocumentCrypto::randomBytes(FileOpenRouter::InviteLink::SeedSize)};
        const std::optional<FileOpenRouter::InviteLink> back = FileOpenRouter::InviteLink::parse(random.toString());
        QVERIFY(back.has_value());
        QCOMPARE(back->documentId, random.documentId);
        QCOMPARE(back->seed, random.seed);
    }
}

void FileOpenRouterTest::aMalformedInviteLinkIsNotOne_data()
{
    const QString seed = VectorLink.mid(VectorLink.lastIndexOf('/') + 1);
    const QString prefix = "wardplanning:join/";

    QTest::addColumn<QString>("argument");
    QTest::newRow("a short seed") << prefix + VectorId + "/" + seed.left(40);
    QTest::newRow("a long seed") << prefix + VectorId + "/" + seed + "AAAA";
    QTest::newRow("a seed with a character outside base64url") << prefix + VectorId + "/" + seed.left(42) + "+";
    QTest::newRow("no seed") << prefix + VectorId;
    QTest::newRow("an empty seed") << prefix + VectorId + "/";
    QTest::newRow("a short id") << prefix + VectorId.left(30) + "/" + seed;
    QTest::newRow("an id in capitals") << prefix + VectorId.toUpper() + "/" + seed;
    QTest::newRow("a path in the id") << prefix + "../" + VectorId + "/" + seed;
    QTest::newRow("a part too many") << prefix + VectorId + "/" + seed + "/more";
    QTest::newRow("the open link no build makes") << "wardplanning:open/" + VectorId;
    QTest::newRow("a web address") << "https://example.test/join/" + VectorId + "/" + seed;
    QTest::newRow("a file") << "C:/wards/" + VectorId + ".wardplanning";
}

void FileOpenRouterTest::aMalformedInviteLinkIsNotOne()
{
    QFETCH(QString, argument);
    QVERIFY(!FileOpenRouter::InviteLink::parse(argument).has_value());

    // Anchor: the vector itself parses, so the refusal is the change's doing.
    QVERIFY(FileOpenRouter::InviteLink::parse(VectorLink).has_value());
}

void FileOpenRouterTest::theInviteMessageSaysWhoWhereAndHow()
{
    const FileOpenRouter::InviteLink link{VectorId, countingSeed()};
    QCOMPARE(FileOpenRouter::inviteMessage("Alice Smith", "Tanglewood Ward", link),
             QString("Alice Smith has invited you to Tanglewood Ward in Ward Planning. Install it from "
                     "https://github.com/jvilburn/EmergencyPlan/releases/latest, then open this link:\n")
                 + VectorLink);
}
