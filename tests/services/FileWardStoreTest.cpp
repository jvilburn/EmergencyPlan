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
