#include "DocumentLibraryTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "DocumentHeader.h"
#include "DocumentLibrary.h"
#include "SessionTestSupport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <utility>

using namespace SessionTestSupport;
using namespace std::placeholders;

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

/// A document sealed only to this device, holding the named ward when one is given.
Document wardDocument(const DeviceIdentity& identity, const QString& wardName)
{
    Document document = Document::empty();
    document.setSealingSet(SealingSet::of(identity.keys()));
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

DocumentFile::Sealed sealedAt(const QString& path)
{
    DocumentFile::Sealed sealed;
    if (DocumentFile::decode(readFile(path), sealed) != DocumentFile::Read::Ok)
    {
        return DocumentFile::Sealed();
    }
    return sealed;
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
    record.baseSaveCounter = 7;
    fixture.settings->setRecord(documentId, record);
    return documentId;
}

void keepRestored(QHash<QString, DocumentSession::Download>& out,
                  const QString& documentId,
                  DocumentSession::Download outcome)
{
    out.insert(documentId, outcome);
}

/// These restores look only at what arrives: the listed set, and any loss, are not
/// under test.
void ignoreListed(const QStringList&)
{
}

void ignoreLost(const QString&, const ServerContract::LostAccess&)
{
}

void keepFetched(std::optional<DocumentSession::Download>& out, const QString&, DocumentSession::Download outcome)
{
    out = outcome;
}

void nameTanglewood(Document& document)
{
    document.metadata().addOrUpdateWard("123456", "Tanglewood", "654321", "Riverside Stake");
}

/// Alice's two shared wards on the server, the first naming its ward.
struct ServerWards
{
    TestWard ward;
    Device* alice = nullptr;
    QString named;
    QString unnamed;
    std::vector<std::unique_ptr<DocumentSession>> sessions;
};

bool shareTwoWards(ServerWards& wards)
{
    wards.alice = &wards.ward.add(newIdentity("Alice", "Laptop"));

    std::unique_ptr<DocumentSession> named = wards.ward.createShared(*wards.alice);
    if (!named)
    {
        return false;
    }
    wards.named = wards.ward.documentId;
    named->edit(DocumentSession::Edit::Directory, nameTanglewood);
    named->publishNow();
    wards.ward.settle();
    wards.sessions.push_back(std::move(named));

    std::unique_ptr<DocumentSession> unnamed = wards.ward.createShared(*wards.alice);
    if (!unnamed)
    {
        return false;
    }
    wards.unnamed = wards.ward.documentId;
    wards.sessions.push_back(std::move(unnamed));

    // Closed, as an app would be before its data was cleared.
    wards.sessions.clear();
    wards.ward.settle();
    return true;
}

bool opens(DocumentLibrary& library, AppSettings& settings, Device& device, const QString& documentId)
{
    LoadFailure failure = LoadFailure::None;
    QString refusal;
    const std::unique_ptr<DocumentSession> session =
        DocumentSession::open(DocumentSession::Context{device.identity, settings, device.connection},
                              std::make_unique<ManualTimers>(),
                              documentId,
                              library.pathsFor(documentId),
                              failure,
                              &refusal);
    if (!session)
    {
        qWarning() << "Did not open:" << refusal;
    }
    return session != nullptr;
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

void DocumentLibraryTest::addingAWardTwiceKeepsItsEntry()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    fixture.library.noteDocument(documentId, wardDocument(fixture.alice, "Tanglewood"));
    fixture.library.markOpened(documentId);
    QVERIFY(fixture.library.exportCopy(documentId,
                                       wardDocument(fixture.alice, "Tanglewood"),
                                       fixture.folder.filePath("copy.wardplanning"),
                                       nullptr));
    const AppSettings::WardEntry before = fixture.settings->ward(documentId).value();
    QVERIFY(before.lastOpened.has_value());
    QCOMPARE(before.copies.size(), qsizetype(1));

    fixture.library.add(documentId);

    QVERIFY(fixture.settings->ward(documentId) == before);
}

void DocumentLibraryTest::aWardAddedFromAFileIsCopiedIn()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    // A ward's own file, found outside the library.
    const QString documentId = DocumentLibrary::newDocumentId();
    DocumentHeader first;
    first.documentId = documentId;
    first.saveCounter = 0;
    const QByteArray bytes = DocumentFile::encode(
        DocumentFile::seal(wardDocument(fixture.alice, "Tanglewood"), first, DocumentCrypto::generateDataKey()).value());
    const QString found = fixture.folder.filePath("found.wardplanning");
    QVERIFY(writeFile(found, bytes));

    QString error;
    QVERIFY2(fixture.library.addFromFile(documentId, found, &error), qPrintable(error));
    QVERIFY(fixture.library.contains(documentId));
    QCOMPARE(readFile(fixture.library.pathsFor(documentId).file), bytes);
    QCOMPARE(readFile(found), bytes);

    // A source that cannot be read adds nothing.
    const QString missingId = DocumentLibrary::newDocumentId();
    QVERIFY(!fixture.library.addFromFile(missingId, fixture.folder.filePath("missing.wardplanning"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!fixture.library.contains(missingId));
}

void DocumentLibraryTest::entriesSortByWardNameThenId()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString beta = DocumentLibrary::newDocumentId();
    const QString alpha = DocumentLibrary::newDocumentId();
    const QString gamma = DocumentLibrary::newDocumentId();
    QString twinLow = DocumentLibrary::newDocumentId();
    QString twinHigh = DocumentLibrary::newDocumentId();
    if (twinHigh < twinLow)
    {
        std::swap(twinLow, twinHigh);
    }

    // Added out of order, so the order read back is the sort's.
    for (const QString& id : {twinHigh, beta, gamma, alpha, twinLow})
    {
        fixture.library.add(id);
    }
    fixture.library.noteDocument(beta, wardDocument(fixture.alice, "beta"));
    fixture.library.noteDocument(alpha, wardDocument(fixture.alice, "Alpha"));
    fixture.library.noteDocument(gamma, wardDocument(fixture.alice, "Gamma"));
    fixture.library.noteDocument(twinHigh, wardDocument(fixture.alice, "Delta"));
    fixture.library.noteDocument(twinLow, wardDocument(fixture.alice, "Delta"));

    // A ward with a base version is shared.
    DocumentRecord shared;
    shared.baseVersion = 1;
    shared.baseHash = DocumentCrypto::payloadHash("base");
    fixture.settings->setRecord(beta, shared);

    const QList<DocumentLibrary::Entry> entries = fixture.library.entries();
    QCOMPARE(entries.size(), qsizetype(5));
    QCOMPARE(entries.at(0).documentId, alpha);
    QCOMPARE(entries.at(0).wardName, QString("Alpha Ward"));
    QVERIFY(!entries.at(0).shared);
    QCOMPARE(entries.at(1).documentId, beta);
    QCOMPARE(entries.at(1).wardName, QString("beta Ward"));
    QVERIFY(entries.at(1).shared);
    QCOMPARE(entries.at(2).documentId, twinLow);
    QCOMPARE(entries.at(3).documentId, twinHigh);
    QCOMPARE(entries.at(3).wardName, QString("Delta Ward"));
    QCOMPARE(entries.at(4).documentId, gamma);
    QCOMPARE(entries.at(4).wardName, QString("Gamma Ward"));
    QVERIFY(!entries.at(4).shared);
}

void DocumentLibraryTest::aDocumentThatNamesItsWardNamesTheEntry()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    QCOMPARE(fixture.library.entries().at(0).wardName, QString("Untitled"));

    // The placeholder is for display only; nothing is stored until the ward is named.
    QVERIFY(!fixture.settings->ward(documentId)->wardName.has_value());

    // A document that does not yet identify its ward leaves the name.
    fixture.library.noteDocument(documentId, wardDocument(fixture.alice, QString()));
    QCOMPARE(fixture.library.entries().at(0).wardName, QString("Untitled"));
    QVERIFY(!fixture.settings->ward(documentId)->wardName.has_value());

    fixture.library.noteDocument(documentId, wardDocument(fixture.alice, "Tanglewood"));
    QCOMPARE(fixture.library.entries().at(0).wardName, QString("Tanglewood Ward"));

    // And it is kept.
    const std::unique_ptr<AppSettings> reread = AppSettings::open(fixture.folder.filePath("settings.json"), nullptr);
    QVERIFY(reread);
    QCOMPARE(reread->ward(documentId)->wardName.value(), QString("Tanglewood Ward"));

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

    // Both survive a write and a reread.
    const std::unique_ptr<AppSettings> reread = AppSettings::open(fixture.folder.filePath("settings.json"), nullptr);
    QVERIFY(reread);
    QCOMPARE(reread->lastWardId(), first);
    QVERIFY(reread->ward(first)->lastOpened == lastOpened);

    // A last ward that has left the library is not offered.
    fixture.settings->setLastWardId(DocumentLibrary::newDocumentId());
    QVERIFY(fixture.library.lastOpenedId().isEmpty());
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

    const DocumentHeader first = sealedAt(firstPath).header;
    const DocumentHeader second = sealedAt(secondPath).header;
    QCOMPARE(first.documentId.size(), qsizetype(DocumentHeader::DocumentIdLength));
    QVERIFY(first.documentId != documentId);
    QVERIFY(second.documentId != documentId);
    QVERIFY(first.documentId != second.documentId);
    QCOMPARE(first.originDocumentId, documentId);
    QCOMPARE(second.originDocumentId, documentId);

    // Sealed to the document's sealing set: Alice's device and no one else.
    QCOMPARE(first.recipients.size(), qsizetype(1));
    QVERIFY(first.recipients.at(0).fingerprint == fixture.alice.fingerprint());

    // It opens with this device's keys, as the document it was written from.
    const std::optional<DocumentLibrary::Copy> copy = fixture.library.openCopy(firstPath, &error);
    QVERIFY2(copy.has_value(), qPrintable(error));
    QCOMPARE(copy->originDocumentId, documentId);
    QVERIFY(copy->document == document);

    // Each is recorded under the ward with its own id.
    QVERIFY(fixture.settings->ward(documentId)->copies
            == (QList<AppSettings::ExportedCopy>{{firstPath, first.documentId}, {secondPath, second.documentId}}));

    // Written again at the same path, the record follows the new copy.
    QVERIFY(fixture.library.exportCopy(documentId, document, firstPath, &error));
    const QString rewritten = sealedAt(firstPath).header.documentId;
    QVERIFY(rewritten != first.documentId);
    QVERIFY(fixture.settings->ward(documentId)->copies
            == (QList<AppSettings::ExportedCopy>{{secondPath, second.documentId}, {firstPath, rewritten}}));

    // A copy is never in the library.
    QCOMPARE(fixture.library.entries().size(), qsizetype(1));
}

void DocumentLibraryTest::aCopyOfAWardNotInTheLibraryIsRefused()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString target = fixture.folder.filePath("copy.wardplanning");
    for (const QString& documentId : {QString(), DocumentLibrary::newDocumentId()})
    {
        QString error;
        QVERIFY(!fixture.library.exportCopy(documentId, wardDocument(fixture.alice, "Tanglewood"), target, &error));
        QCOMPARE(error, QString("Only a ward on this device can be copied."));
        QVERIFY(!QFile::exists(target));
    }

    // Nothing was written that would stop the settings opening.
    QVERIFY(fixture.settings->wardIds().isEmpty());
    QVERIFY(AppSettings::open(fixture.folder.filePath("settings.json"), nullptr));
}

void DocumentLibraryTest::aCopyIntoTheWardsDirectoryIsRefused()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString documentId = addWardWithFiles(fixture);
    const QString wardFile = fixture.library.pathsFor(documentId).file;
    const QByteArray wardBytes = readFile(wardFile);
    const Document document = wardDocument(fixture.alice, "Tanglewood");

    for (const QString& target : {wardFile, QDir(fixture.folder.filePath("wards")).filePath("copy.wardplanning")})
    {
        QString error;
        QVERIFY(!fixture.library.exportCopy(documentId, document, target, &error));
        QCOMPARE(error, QString("A copy cannot be saved among the wards' own files. Choose another folder."));
    }
    QCOMPARE(readFile(wardFile), wardBytes);
    QVERIFY(fixture.settings->ward(documentId)->copies.isEmpty());

    // Anchor: the folder beside it is fine.
    QVERIFY(fixture.library.exportCopy(documentId, document, fixture.folder.filePath("copy.wardplanning"), nullptr));
}

void DocumentLibraryTest::openCopySaysWhyItCannotOpenAFile()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    const Document document = wardDocument(fixture.alice, "Tanglewood");
    const QString copyPath = fixture.folder.filePath("copy.wardplanning");
    QVERIFY(fixture.library.exportCopy(documentId, document, copyPath, nullptr));
    const DocumentFile::Sealed copy = sealedAt(copyPath);

    // Anchor: Alice opens it.
    QString error;
    QVERIFY2(fixture.library.openCopy(copyPath, &error).has_value(), qPrintable(error));

    // Bob is not among its recipients.
    QTemporaryDir bobFolder;
    QVERIFY(bobFolder.isValid());
    const std::unique_ptr<AppSettings> bobSettings = AppSettings::open(bobFolder.filePath("settings.json"), nullptr);
    const DocumentLibrary bob(*bobSettings, bobFolder.filePath("wards"), newIdentity("Bob", "Phone"));
    QVERIFY(!bob.openCopy(copyPath, &error).has_value());
    QCOMPARE(error, QString("This device is not among those %1 was written for.").arg(copyPath));

    // One payload byte changed.
    DocumentFile::Sealed flipped = copy;
    flipped.ciphertext[flipped.ciphertext.size() / 2] = char(flipped.ciphertext.at(flipped.ciphertext.size() / 2) ^ 0x01);
    const QString flippedPath = fixture.folder.filePath("flipped.wardplanning");
    QVERIFY(writeFile(flippedPath, DocumentFile::encode(flipped)));
    QVERIFY(!fixture.library.openCopy(flippedPath, &error).has_value());
    QCOMPARE(error, QString("%1 is damaged, or was changed after it was written.").arg(flippedPath));

    // A ward's own file: no origin.
    DocumentHeader own;
    own.documentId = DocumentLibrary::newDocumentId();
    own.saveCounter = 0;
    const QString ownPath = fixture.folder.filePath("own.wardplanning");
    QVERIFY(writeFile(ownPath,
                      DocumentFile::encode(
                          DocumentFile::seal(document, own, DocumentCrypto::generateDataKey()).value())));
    QVERIFY(!fixture.library.openCopy(ownPath, &error).has_value());
    QCOMPARE(error, QString("%1 is a ward's own file, not a copy.").arg(ownPath));

    // A later format.
    DocumentFile::Sealed later = copy;
    later.header.formatVersion = DocumentHeader::CurrentFormatVersion + 1;
    const QString laterPath = fixture.folder.filePath("later.wardplanning");
    QVERIFY(writeFile(laterPath, DocumentFile::encode(later)));
    QVERIFY(!fixture.library.openCopy(laterPath, &error).has_value());
    QCOMPARE(error, QString("%1 was written by a newer version of Ward Planning.").arg(laterPath));

    const QString notAWard = fixture.folder.filePath("notes.wardplanning");
    QVERIFY(writeFile(notAWard, "shopping list"));
    QVERIFY(!fixture.library.openCopy(notAWard, &error).has_value());
    QCOMPARE(error, QString("%1 is not a Ward Planning file.").arg(notAWard));

    const QString missing = fixture.folder.filePath("missing.wardplanning");
    QVERIFY(!fixture.library.openCopy(missing, &error).has_value());
    QCOMPARE(error, QString("%1 could not be read.").arg(missing));
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

void DocumentLibraryTest::forgetLostDeletesCopiesStillAtTheirPaths()
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
    const QString otherWard = fixture.folder.filePath("other ward.wardplanning");
    const QString sameWard = fixture.folder.filePath("same ward.wardplanning");
    QVERIFY(fixture.library.exportCopy(removed, document, stillThere, nullptr));
    QVERIFY(fixture.library.exportCopy(removed, document, movedFrom, nullptr));
    QVERIFY(fixture.library.exportCopy(removed, document, otherWard, nullptr));
    QVERIFY(fixture.library.exportCopy(removed, document, sameWard, nullptr));

    // One copy moved by the person. One path now holds a copy of a different ward,
    // another a different copy of this same ward, such as one sent by another member.
    QVERIFY(QFile::rename(movedFrom, movedTo));
    QVERIFY(QFile::remove(otherWard));
    QVERIFY(fixture.library.exportCopy(other, document, otherWard, nullptr));
    const QByteArray otherWardBytes = readFile(otherWard);
    const QString sentPath = fixture.folder.filePath("sent.wardplanning");
    QVERIFY(fixture.library.exportCopy(removed, document, sentPath, nullptr));
    QVERIFY(QFile::remove(sameWard));
    QVERIFY(QFile::rename(sentPath, sameWard));
    const QByteArray sameWardBytes = readFile(sameWard);
    QCOMPARE(sealedAt(sameWard).header.originDocumentId, removed);

    fixture.library.forgetLost(removed);

    QVERIFY(!QFile::exists(stillThere));
    QVERIFY(QFile::exists(movedTo));
    QCOMPARE(sealedAt(movedTo).header.originDocumentId, removed);
    QCOMPARE(readFile(otherWard), otherWardBytes);
    QCOMPARE(readFile(sameWard), sameWardBytes);

    QVERIFY(!QFile::exists(paths.file));
    QVERIFY(!QFile::exists(paths.base));
    QVERIFY(!fixture.settings->documentIds().contains(removed));
    QVERIFY(!fixture.settings->ward(removed).has_value());

    // The other ward keeps its entry and the copy recorded under it.
    QCOMPARE(fixture.settings->ward(other)->copies.size(), qsizetype(1));
    QCOMPARE(fixture.settings->ward(other)->copies.at(0).path, otherWard);
}

void DocumentLibraryTest::aClearedLibraryIsRestoredFromTheServerAndEachOpens()
{
    ServerWards wards;
    QVERIFY(shareTwoWards(wards));

    // The same device, its settings and library cleared.
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library(*settings, folder.filePath("wards"), wards.alice->identity);
    QVERIFY(library.entries().isEmpty());

    QObject receiver;
    QHash<QString, DocumentSession::Download> restored;
    library.restore(wards.alice->connection, &receiver, &ignoreListed, std::bind(&keepRestored, std::ref(restored), _1, _2), &ignoreLost);
    wards.ward.settle();

    QCOMPARE(restored.size(), qsizetype(2));
    QVERIFY(restored.value(wards.named) == DocumentSession::Download::Done);
    QVERIFY(restored.value(wards.unnamed) == DocumentSession::Download::Done);

    const QList<DocumentLibrary::Entry> entries = library.entries();
    QCOMPARE(entries.size(), qsizetype(2));
    for (const DocumentLibrary::Entry& entry : entries)
    {
        QVERIFY(entry.shared);
        QVERIFY(entry.onDevice);
        QCOMPARE(entry.wardName, entry.documentId == wards.named ? QString("Tanglewood Ward") : QString("Untitled"));
        QVERIFY(opens(library, *settings, *wards.alice, entry.documentId));
    }

    // Once restored, nothing more is fetched.
    restored.clear();
    library.restore(wards.alice->connection, &receiver, &ignoreListed, std::bind(&keepRestored, std::ref(restored), _1, _2), &ignoreLost);
    wards.ward.settle();
    QVERIFY(restored.isEmpty());
}

void DocumentLibraryTest::aListedWardWhoseFileWasDeletedIsFetchedOnRequest()
{
    ServerWards wards;
    QVERIFY(shareTwoWards(wards));

    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library(*settings, folder.filePath("wards"), wards.alice->identity);
    QObject receiver;
    QHash<QString, DocumentSession::Download> restored;
    library.restore(wards.alice->connection, &receiver, &ignoreListed, std::bind(&keepRestored, std::ref(restored), _1, _2), &ignoreLost);
    wards.ward.settle();
    QCOMPARE(restored.size(), qsizetype(2));

    const QString file = library.pathsFor(wards.named).file;
    QVERIFY(QFile::remove(file));
    for (const DocumentLibrary::Entry& entry : library.entries())
    {
        QCOMPARE(entry.onDevice, entry.documentId != wards.named);
    }

    std::optional<DocumentSession::Download> fetched;
    library.fetchWard(wards.named, wards.alice->connection, &receiver, std::bind(&keepFetched, std::ref(fetched), _1, _2));
    wards.ward.settle();

    QVERIFY(fetched == DocumentSession::Download::Done);
    QVERIFY(QFile::exists(file));
    QVERIFY(opens(library, *settings, *wards.alice, wards.named));
}

void DocumentLibraryTest::aRecoveryCopyOpensForEverySealedDeviceAndIsRecorded()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());

    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    fixture.library.noteDocument(documentId, wardDocument(fixture.alice, "Tanglewood"));

    // The sealing set after a refused publish: Alice and the Bob they granted.
    const DeviceIdentity bob = newIdentity("Bob", "Phone");
    Document document = wardDocument(fixture.alice, "Tanglewood");
    SealingSet set = document.sealingSet();
    QVERIFY(set.add(bob.keys()));
    document.setSealingSet(set);

    QString path;
    QString error;
    QVERIFY2(fixture.library.writeRecoveryCopy(documentId, document, fixture.folder.filePath("Documents"), &path, &error),
             qPrintable(error));
    QVERIFY(QFileInfo(path).fileName().startsWith("Tanglewood Ward (unsaved changes "));
    QVERIFY(path.endsWith(".wardplanning"));
    QCOMPARE(QFileInfo(path).absolutePath(), QFileInfo(fixture.folder.filePath("Documents")).absoluteFilePath());

    const DocumentHeader header = sealedAt(path).header;
    QVERIFY(header.documentId != documentId);
    QCOMPARE(header.originDocumentId, documentId);
    QCOMPARE(header.recipients.size(), qsizetype(2));

    // Every device it is sealed to opens it, and it holds the document given.
    const std::optional<DocumentLibrary::Copy> copy = fixture.library.openCopy(path, &error);
    QVERIFY2(copy.has_value(), qPrintable(error));
    QVERIFY(copy->document == document);
    QCOMPARE(copy->originDocumentId, documentId);
    QTemporaryDir bobFolder;
    QVERIFY(bobFolder.isValid());
    const std::unique_ptr<AppSettings> bobSettings = AppSettings::open(bobFolder.filePath("settings.json"), nullptr);
    const DocumentLibrary bobLibrary(*bobSettings, bobFolder.filePath("wards"), bob);
    QVERIFY2(bobLibrary.openCopy(path, &error).has_value(), qPrintable(error));

    // Recorded like an exported copy, so a removal deletes it too.
    QCOMPARE(fixture.settings->ward(documentId)->copies.size(), qsizetype(1));
    QCOMPARE(fixture.settings->ward(documentId)->copies.at(0).path, path);
}

void DocumentLibraryTest::aSecondRecoveryCopyInTheSameMinuteKeepsTheFirst()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());
    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    fixture.library.noteDocument(documentId, wardDocument(fixture.alice, "Tanglewood"));
    const QString documents = fixture.folder.filePath("Documents");

    QString first;
    QString second;
    QVERIFY(fixture.library.writeRecoveryCopy(documentId, wardDocument(fixture.alice, "Tanglewood"), documents, &first, nullptr));
    QVERIFY(fixture.library.writeRecoveryCopy(documentId, wardDocument(fixture.alice, "Oakridge"), documents, &second, nullptr));

    // In the same minute, the second is numbered rather than written over the first.
    QVERIFY(first != second);
    QVERIFY(QFile::exists(first));
    QVERIFY(QFile::exists(second));
    QVERIFY(sealedAt(first).header.documentId != sealedAt(second).header.documentId);
    QCOMPARE(fixture.settings->ward(documentId)->copies.size(), qsizetype(2));
}

void DocumentLibraryTest::aRecoveryCopyNameIsOneAnyFileSystemTakes()
{
    Fixture fixture;
    QVERIFY(fixture.folder.isValid());
    const QString documentId = DocumentLibrary::newDocumentId();
    fixture.library.add(documentId);
    AppSettings::WardEntry entry = fixture.settings->ward(documentId).value();
    entry.wardName = "Elm/Oak: North*.";
    fixture.settings->setWard(documentId, entry);

    QString path;
    QString error;
    QVERIFY2(fixture.library.writeRecoveryCopy(documentId,
                                               wardDocument(fixture.alice, "Elm"),
                                               fixture.folder.filePath("Documents"),
                                               &path,
                                               &error),
             qPrintable(error));
    const QString name = QFileInfo(path).fileName();
    QVERIFY2(name.startsWith("Elm-Oak- North- (unsaved changes "), qPrintable(name));
    QVERIFY(QFile::exists(path));
}

void DocumentLibraryTest::onlyAWardThatArrivesIsAdded()
{
    ServerWards wards;
    QVERIFY(shareTwoWards(wards));
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library(*settings, folder.filePath("wards"), wards.alice->identity);
    QObject receiver;

    // Out of reach, nothing is added, so the next restore tries it afresh.
    wards.ward.server.setOffline(true);
    std::optional<DocumentSession::Download> fetched;
    library.fetchWard(wards.named, wards.alice->connection, &receiver, std::bind(&keepFetched, std::ref(fetched), _1, _2));
    wards.ward.settle();
    QVERIFY(fetched == DocumentSession::Download::Unreachable);
    QVERIFY(!library.contains(wards.named));
    QVERIFY(library.entries().isEmpty());

    wards.ward.server.setOffline(false);
    QHash<QString, DocumentSession::Download> restored;
    library.restore(wards.alice->connection, &receiver, &ignoreListed, std::bind(&keepRestored, std::ref(restored), _1, _2), &ignoreLost);
    wards.ward.settle();
    QVERIFY(restored.value(wards.named) == DocumentSession::Download::Done);
    QVERIFY(library.contains(wards.named));
}

void DocumentLibraryTest::aWardWhoseFetchIsCutShortIsNotAdded()
{
    ServerWards wards;
    QVERIFY(shareTwoWards(wards));
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library(*settings, folder.filePath("wards"), wards.alice->identity);

    // The window closes while the fetch is on its way.
    std::unique_ptr<QObject> receiver = std::make_unique<QObject>();
    QHash<QString, DocumentSession::Download> restored;
    library.restore(wards.alice->connection, receiver.get(), &ignoreListed, std::bind(&keepRestored, std::ref(restored), _1, _2), &ignoreLost);
    receiver.reset();
    wards.ward.settle();

    QVERIFY(restored.isEmpty());
    QVERIFY(library.entries().isEmpty());

    // Anchor: the next start fetches both.
    QObject next;
    library.restore(wards.alice->connection, &next, &ignoreListed, std::bind(&keepRestored, std::ref(restored), _1, _2), &ignoreLost);
    wards.ward.settle();
    QCOMPARE(library.entries().size(), qsizetype(2));
}

void DocumentLibraryTest::aWardLostWhileFetchedKeepsItsEntryForForgetLost()
{
    TestWard ward;
    Device& alice = ward.add(newIdentity("Alice", "Laptop"));
    Device& bob = ward.add(newIdentity("Bob", "Phone"));
    std::unique_ptr<DocumentSession> aliceSession = ward.createShared(alice);
    QVERIFY(aliceSession);
    QVERIFY(ward.grant(alice, bob));

    // Bob's library holds the ward, named, with a copy written from it.
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    DocumentLibrary library(*settings, folder.filePath("wards"), bob.identity);
    QObject receiver;
    std::optional<DocumentSession::Download> fetched;
    library.fetchWard(ward.documentId, bob.connection, &receiver, std::bind(&keepFetched, std::ref(fetched), _1, _2));
    ward.settle();
    QVERIFY(fetched == DocumentSession::Download::Done);
    AppSettings::WardEntry entry = settings->ward(ward.documentId).value();
    entry.wardName = "Tanglewood Ward";
    settings->setWard(ward.documentId, entry);
    const QString copyPath = folder.filePath("copy.wardplanning");
    QVERIFY(library.exportCopy(ward.documentId, wardDocument(bob.identity, "Tanglewood"), copyPath, nullptr));

    // Alice removes Bob; Bob's file is gone, so opening it fetches.
    const ServerContract::Session admin = ward.sessionFor(alice).value();
    QVERIFY(ServerTestSupport::wait<ServerContract::Done>(std::bind(&ServerContract::ServerApi::removeMember,
                                                                    &ward.server,
                                                                    admin,
                                                                    ward.documentId,
                                                                    bob.identity.member,
                                                                    std::placeholders::_1))
                .ok());
    QVERIFY(QFile::remove(library.pathsFor(ward.documentId).file));

    fetched.reset();
    library.fetchWard(ward.documentId, bob.connection, &receiver, std::bind(&keepFetched, std::ref(fetched), _1, _2));
    ward.settle();
    QVERIFY(fetched == DocumentSession::Download::AccessLost);

    // The entry is still there to name the ward and find its copies.
    QVERIFY(library.contains(ward.documentId));
    QCOMPARE(library.entries().at(0).wardName, QString("Tanglewood Ward"));
    library.forgetLost(ward.documentId);
    QVERIFY(!QFile::exists(copyPath));
    QVERIFY(!library.contains(ward.documentId));
}
