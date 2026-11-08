#include "UserFolderTest.h"
#include "UserFolder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

namespace
{

QByteArray contentsOf(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QByteArray();
    }
    return file.readAll();
}

}  // namespace

UserFolderTest::UserFolderTest(QObject* parent)
    : QObject(parent)
{
}

void UserFolderTest::writeMakesTheFolderAndReplacesAFileOfItsName()
{
    QTemporaryDir documents;
    QVERIFY(documents.isValid());
    UserFolder folder(documents.path() + "/Ward Planning");
    QString problem;

    QString first = folder.write("Tanglewood Ward 2026-11-08.wardupdate", "first", &problem);
    QString second = folder.write("Tanglewood Ward 2026-11-08.wardupdate", "second", &problem);

    QCOMPARE(problem, QString());
    QCOMPARE(first, documents.path() + "/Ward Planning/Tanglewood Ward 2026-11-08.wardupdate");
    QCOMPARE(second, first);
    QCOMPARE(contentsOf(first), QByteArray("second"));
    QCOMPARE(QDir(folder.path()).entryList(QDir::Files), QStringList({"Tanglewood Ward 2026-11-08.wardupdate"}));
}

void UserFolderTest::writeNewNumbersEachFileWhoseNameIsTaken()
{
    QTemporaryDir documents;
    QVERIFY(documents.isValid());
    UserFolder folder(documents.path() + "/Ward Planning");
    QString base = "To redo - Tanglewood Ward - 2026-11-08 14.05";
    QString problem;

    QString first = folder.writeNew(base, ".txt", "first", &problem);
    QString second = folder.writeNew(base, ".txt", "second", &problem);
    QString third = folder.writeNew(base, ".txt", "third", &problem);

    QCOMPARE(problem, QString());
    QCOMPARE(QFileInfo(first).fileName(), base + ".txt");
    QCOMPARE(QFileInfo(second).fileName(), base + " (2).txt");
    QCOMPARE(QFileInfo(third).fileName(), base + " (3).txt");
    QCOMPARE(contentsOf(first), QByteArray("first"));
    QCOMPARE(contentsOf(second), QByteArray("second"));
    QCOMPARE(contentsOf(third), QByteArray("third"));
}
