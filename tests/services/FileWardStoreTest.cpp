#include "FileWardStoreTest.h"
#include "FileWardStore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

FileWardStoreTest::FileWardStoreTest(QObject* parent)
    : QObject(parent)
{
}

void FileWardStoreTest::eachWardIsOneFileNamedByItsDocumentId()
{
    QTemporaryDir root;
    QString directory = root.path() + "/wards";
    FileWardStore store(directory);
    DocumentId document = DocumentId::generate();
    QString problem;

    QVERIFY(store.write(document, QByteArray("first\nbytes"), &problem));
    QVERIFY(store.write(document, QByteArray("second\r\nbytes\n"), &problem));

    QCOMPARE(store.pathOf(document), directory + "/" + document.toString() + ".wardplanning");
    QCOMPARE(QDir(directory).entryList(QDir::Files), QStringList({document.toString() + ".wardplanning"}));
    QFile file(store.pathOf(document));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("second\r\nbytes\n"));
    StoreRead read = store.read(document);
    QVERIFY(read.status == StoreReadStatus::Read);
    QCOMPARE(read.bytes, QByteArray("second\r\nbytes\n"));
    QVERIFY(store.read(DocumentId::generate()).status == StoreReadStatus::Missing);
}

void FileWardStoreTest::writeFailuresSayWhyAsAClause()
{
    QTemporaryDir root;
    QFile blocker(root.path() + "/blocker");
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    QString unmadeDirectory = root.path() + "/blocker/wards";
    FileWardStore unmade(unmadeDirectory);
    QString problem;

    QVERIFY(!unmade.write(DocumentId::generate(), QByteArray("bytes"), &problem));
    QCOMPARE(problem, QString("the folder %1 could not be made").arg(QDir::toNativeSeparators(unmadeDirectory)));

    // The ward's file held open, so the system refuses to replace it and says why.
    FileWardStore store(root.path() + "/wards");
    DocumentId document = DocumentId::generate();
    QVERIFY(store.write(document, QByteArray("first"), &problem));
    QFile held(store.pathOf(document));
    QVERIFY(held.open(QIODevice::ReadOnly));
    problem.clear();

    QVERIFY(!store.write(document, QByteArray("second"), &problem));
    QVERIFY(!problem.isEmpty());
    QVERIFY2(!problem.endsWith('.'), qPrintable(problem));
}
