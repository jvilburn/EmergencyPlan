#include "DocumentLibraryTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "DocumentHeader.h"
#include "DocumentLibrary.h"
#include "SessionTestSupport.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace SessionTestSupport;

namespace
{

/// Alice's library over a fresh folder: settings beside the wards' directory.
struct Fixture
{
    QTemporaryDir folder;
    DeviceIdentity alice = newIdentity("Alice", "Laptop");
    std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library{*settings, folder.filePath("wards"), alice};
};

/// A document whose roster is only this device's person, holding the named ward
/// when one is given.
Document wardDocument(const DeviceIdentity& identity, const QString& wardName)
{
    AccessRoster roster;
    roster.addPerson(identity.asNewPerson());

    Document document = Document::empty();
    document.setAccessRoster(roster);
    if (!wardName.isEmpty())
    {
        document.metadata().addOrUpdateWard("123456", wardName, "654321", "Riverside Stake");
    }
    return document;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

DocumentHeader headerAt(const QString& path)
{
    DocumentFile::Sealed sealed;
    if (DocumentFile::decode(readFile(path), sealed) != DocumentFile::Read::Ok)
    {
        return DocumentHeader();
    }
    return sealed.header;
}

/// Adds a ward with both its files on disk and a record, as a ward that has been
/// opened and shared would have.
QString addWardWithFiles(Fixture& fixture)
{
    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);

    const DocumentPaths paths = fixture.library.pathsFor(documentId);
    writeFile(paths.file, "the ward's file");
    writeFile(paths.base, "the ward's base copy");

    DocumentRecord record;
    record.baseVersion = 4;
    record.baseHash = DocumentCrypto::payloadHash("the ward's base copy");
    record.highWaterMark.keyGeneration = 1;
    record.highWaterMark.saveCounter = 7;
    fixture.settings->setRecord(documentId, record);
    return documentId;
}

} // namespace

void DocumentLibraryTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void DocumentLibraryTest::newIdsAreDistinctAndPathsAreUnderTheDirectory()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QRegularExpression hex128("^[0-9a-f]{32}$");
    QSet<QString> ids;
    for (int i = 0; i < 100; ++i)
    {
        const QString id = DocumentLibrary::newDocumentId();
        QVERIFY2(hex128.match(id).hasMatch(), qPrintable(id));
        ids.insert(id);
    }
    QCOMPARE(ids.size(), qsizetype(100));

    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    const DocumentPaths paths = fixture.library.pathsFor(documentId);
    const QDir wards(fixture.folder.filePath("wards"));
    QCOMPARE(paths.file, wards.filePath(documentId + ".wardplanning"));
    QCOMPARE(paths.base, wards.filePath(documentId + ".base"));
    QVERIFY(paths.file != paths.base);

    // Adding makes the directory, so the first save has somewhere to go.
    QVERIFY(wards.exists());
}

void DocumentLibraryTest::pathsForAnUnknownIdAreEmpty()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString known = DocumentLibrary::newDocumentId();
    fixture.library.add(known);
    QVERIFY(!fixture.library.pathsFor(known).file.isEmpty());

    const DocumentPaths unknown = fixture.library.pathsFor(DocumentLibrary::newDocumentId());
    QVERIFY(unknown.file.isEmpty());
    QVERIFY(unknown.base.isEmpty());
}

void DocumentLibraryTest::entriesSortByWardName()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString beta = DocumentLibrary::newDocumentId();
    const QString alpha = DocumentLibrary::newDocumentId();
    const QString gamma = DocumentLibrary::newDocumentId();
    fixture.library.add(beta);
    fixture.library.add(alpha);
    fixture.library.add(gamma);
    fixture.library.noteDocument(beta, wardDocument(fixture.alice, "beta"));
    fixture.library.noteDocument(alpha, wardDocument(fixture.alice, "Alpha"));
    fixture.library.noteDocument(gamma, wardDocument(fixture.alice, "Gamma"));

    // A ward with a base version is shared.
    DocumentRecord shared;
    shared.baseVersion = 1;
    shared.baseHash = DocumentCrypto::payloadHash("base");
    fixture.settings->setRecord(beta, shared);

    const QList<DocumentLibrary::Entry> entries = fixture.library.entries();
    QCOMPARE(entries.size(), qsizetype(3));
    QCOMPARE(entries.at(0).documentId, alpha);
    QCOMPARE(entries.at(0).wardName, QString("Alpha Ward"));
    QVERIFY(!entries.at(0).shared);
    QCOMPARE(entries.at(1).documentId, beta);
    QCOMPARE(entries.at(1).wardName, QString("beta Ward"));
    QVERIFY(entries.at(1).shared);
    QCOMPARE(entries.at(2).documentId, gamma);
    QCOMPARE(entries.at(2).wardName, QString("Gamma Ward"));
    QVERIFY(!entries.at(2).shared);
}

void DocumentLibraryTest::aDocumentThatNamesItsWardNamesTheEntry()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    QCOMPARE(fixture.library.entries().at(0).wardName, QString("Untitled"));

    // A document that does not yet identify its ward leaves the name.
    fixture.library.noteDocument(documentId, wardDocument(fixture.alice, QString()));
    QCOMPARE(fixture.library.entries().at(0).wardName, QString("Untitled"));

    fixture.library.noteDocument(documentId, wardDocument(fixture.alice, "Tanglewood"));
    QCOMPARE(fixture.library.entries().at(0).wardName, QString("Tanglewood Ward"));

    // And it is kept.
    const std::unique_ptr<AppSettings> reread = AppSettings::open(fixture.folder.filePath("settings.json"), nullptr);
    QVERIFY(reread);
    QCOMPARE(reread->ward(documentId)->wardName, QString("Tanglewood Ward"));

    // A ward not in the library is not added by naming it.
    const QString stranger = DocumentLibrary::newDocumentId();
    fixture.library.noteDocument(stranger, wardDocument(fixture.alice, "Elsewhere"));
    QVERIFY(!fixture.settings->ward(stranger).has_value());
}

void DocumentLibraryTest::openingAWardMarksItToReopen()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString first = DocumentLibrary::newDocumentId();
    const QString second = DocumentLibrary::newDocumentId();
    fixture.library.add(first);
    fixture.library.add(second);
    QVERIFY(fixture.library.lastOpenedId().isEmpty());
    QVERIFY(!fixture.settings->ward(first)->lastOpened.has_value());

    const QDateTime before = QDateTime::currentDateTimeUtc();
    fixture.library.markOpened(first);
    const QDateTime after = QDateTime::currentDateTimeUtc();
    fixture.library.markOpened(second);
    fixture.library.markOpened(first);

    QCOMPARE(fixture.library.lastOpenedId(), first);
    const std::optional<QDateTime> lastOpened = fixture.settings->ward(first)->lastOpened;
    QVERIFY(lastOpened.has_value());
    QVERIFY(lastOpened.value() >= before);
    QVERIFY(fixture.settings->ward(second)->lastOpened.value() >= after);

    // An id not in the library is not remembered.
    fixture.library.markOpened(DocumentLibrary::newDocumentId());
    QCOMPARE(fixture.library.lastOpenedId(), first);

    const std::unique_ptr<AppSettings> reread = AppSettings::open(fixture.folder.filePath("settings.json"), nullptr);
    QVERIFY(reread);
    QCOMPARE(reread->lastWardId(), first);
}

void DocumentLibraryTest::anExportedCopyHasAFreshIdAndTheWardAsItsOrigin()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    const Document document = wardDocument(fixture.alice, "Tanglewood");

    const QString firstPath = fixture.folder.filePath("first copy.wardplanning");
    const QString secondPath = fixture.folder.filePath("second copy.wardplanning");
    QString error;
    QVERIFY2(fixture.library.exportCopy(documentId, document, firstPath, &error), qPrintable(error));
    QVERIFY2(fixture.library.exportCopy(documentId, document, secondPath, &error), qPrintable(error));

    const DocumentHeader first = headerAt(firstPath);
    const DocumentHeader second = headerAt(secondPath);
    QCOMPARE(first.documentId.size(), qsizetype(DocumentHeader::DocumentIdLength));
    QVERIFY(first.documentId != documentId);
    QVERIFY(second.documentId != documentId);
    QVERIFY(first.documentId != second.documentId);
    QCOMPARE(first.originDocumentId, documentId);
    QCOMPARE(second.originDocumentId, documentId);

    // Each opens with this device's keys, as the document it was written from.
    const std::optional<DocumentLibrary::Copy> copy = fixture.library.openCopy(firstPath, &error);
    QVERIFY2(copy.has_value(), qPrintable(error));
    QCOMPARE(copy->originDocumentId, documentId);
    QVERIFY(copy->document == document);

    // Both are recorded under the ward, once each, however often written.
    QVERIFY(fixture.library.exportCopy(documentId, document, firstPath, &error));
    QCOMPARE(fixture.settings->ward(documentId)->copyPaths, (QStringList{firstPath, secondPath}));

    // A copy is never in the library.
    QCOMPARE(fixture.library.entries().size(), qsizetype(1));
}

void DocumentLibraryTest::aCopyForAnotherDeviceIsRefused()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    const QString copyPath = fixture.folder.filePath("copy.wardplanning");
    QVERIFY(fixture.library.exportCopy(documentId, wardDocument(fixture.alice, "Tanglewood"), copyPath, nullptr));

    // Anchor: Alice opens it.
    QString error;
    QVERIFY2(fixture.library.openCopy(copyPath, &error).has_value(), qPrintable(error));

    QTemporaryDir bobFolder;
    QVERIFY(bobFolder.isValid());
    const std::unique_ptr<AppSettings> bobSettings = AppSettings::open(bobFolder.filePath("settings.json"), nullptr);
    const DocumentLibrary bob(*bobSettings, bobFolder.filePath("wards"), newIdentity("Bob", "Phone"));
    QVERIFY(!bob.openCopy(copyPath, &error).has_value());
    QVERIFY(error.contains(copyPath));

    const QString notAWard = fixture.folder.filePath("notes.wardplanning");
    QVERIFY(writeFile(notAWard, "shopping list"));
    error.clear();
    QVERIFY(!fixture.library.openCopy(notAWard, &error).has_value());
    QVERIFY(error.contains(notAWard));

    error.clear();
    QVERIFY(!fixture.library.openCopy(fixture.folder.filePath("missing.wardplanning"), &error).has_value());
    QVERIFY(!error.isEmpty());
}

void DocumentLibraryTest::forgetRemovesTheFilesRecordAndEntryAndKeepsCopies()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString forgotten = addWardWithFiles(fixture);
    const QString kept = addWardWithFiles(fixture);
    const DocumentPaths paths = fixture.library.pathsFor(forgotten);
    const QString copyPath = fixture.folder.filePath("copy.wardplanning");
    QVERIFY(fixture.library.exportCopy(forgotten, wardDocument(fixture.alice, "Tanglewood"), copyPath, nullptr));
    fixture.library.markOpened(forgotten);

    fixture.library.forget(forgotten);

    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(!fixture.settings->documentIds().contains(forgotten));
    QVERIFY(!fixture.settings->ward(forgotten).has_value());
    QVERIFY(fixture.library.lastOpenedId().isEmpty());
    QVERIFY(fixture.library.pathsFor(forgotten).file.isEmpty());

    // The copy is the person's to keep.
    QVERIFY(QFile::exists(copyPath));

    // The other ward is untouched.
    QCOMPARE(fixture.library.entries().size(), qsizetype(1));
    QCOMPARE(fixture.library.entries().at(0).documentId, kept);
    QVERIFY(QFile::exists(fixture.library.pathsFor(kept).file));
    QVERIFY(fixture.settings->record(kept).baseVersion.has_value());
}

void DocumentLibraryTest::forgetRemovedDeletesCopiesStillAtTheirPaths()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString removed = addWardWithFiles(fixture);
    const QString other = addWardWithFiles(fixture);
    const DocumentPaths paths = fixture.library.pathsFor(removed);
    const Document document = wardDocument(fixture.alice, "Tanglewood");

    const QString stillThere = fixture.folder.filePath("still there.wardplanning");
    const QString movedFrom = fixture.folder.filePath("moved from.wardplanning");
    const QString movedTo = fixture.folder.filePath("moved to.wardplanning");
    const QString replaced = fixture.folder.filePath("replaced.wardplanning");
    QVERIFY(fixture.library.exportCopy(removed, document, stillThere, nullptr));
    QVERIFY(fixture.library.exportCopy(removed, document, movedFrom, nullptr));
    QVERIFY(fixture.library.exportCopy(removed, document, replaced, nullptr));

    // One copy moved by the person; another path now holds a copy of a different ward.
    QVERIFY(QFile::rename(movedFrom, movedTo));
    QVERIFY(QFile::remove(replaced));
    QVERIFY(fixture.library.exportCopy(other, document, replaced, nullptr));
    const QByteArray otherCopy = readFile(replaced);

    fixture.library.forgetRemoved(removed);

    QVERIFY(!QFile::exists(stillThere));
    QVERIFY(QFile::exists(movedTo));
    QCOMPARE(headerAt(movedTo).originDocumentId, removed);
    QCOMPARE(readFile(replaced), otherCopy);

    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(!fixture.settings->documentIds().contains(removed));
    QVERIFY(!fixture.settings->ward(removed).has_value());

    // The other ward keeps its entry and the copy recorded under it.
    QCOMPARE(fixture.settings->ward(other)->copyPaths, QStringList{replaced});
}
