#include "LockTest.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTest>

namespace
{

constexpr int Timeout = 30000;

/// The next line copy prints, waiting for it.
QString nextLine(QProcess& copy)
{
    while (!copy.canReadLine() && copy.waitForReadyRead(Timeout))
    {
    }
    return QString::fromUtf8(copy.readLine()).trimmed();
}

}  // namespace

LockTest::LockTest(QObject* parent)
    : QObject(parent)
{
}

void LockTest::init()
{
    QVERIFY(m_folder.isValid());
    m_lockPath = m_folder.path() + "/" + QTest::currentTestFunction() + "/instance.lock";
}

QString LockTest::hold(QProcess& copy) const
{
    return start(copy, {"hold", m_lockPath});
}

QString LockTest::hand(QProcess& copy, const QString& message) const
{
    return start(copy, {"hand", m_lockPath, message});
}

QString LockTest::start(QProcess& copy, const QStringList& arguments) const
{
    copy.start(QCoreApplication::applicationFilePath(), arguments);
    return nextLine(copy);
}

void LockTest::secondProcessIsRefusedWhileTheFirstHoldsTheLock()
{
    QProcess holder;
    QCOMPARE(hold(holder), QString("held"));

    QProcess second;
    QCOMPARE(hold(second), QString("refused"));
    QVERIFY(second.waitForFinished(Timeout));
    QCOMPARE(second.exitCode(), 1);

    holder.kill();
    QVERIFY(holder.waitForFinished(Timeout));
}

void LockTest::lockIsTakenOnceTheHolderIsKilled()
{
    QProcess holder;
    QCOMPARE(hold(holder), QString("held"));
    QProcess refused;
    QCOMPARE(hold(refused), QString("refused"));
    QVERIFY(refused.waitForFinished(Timeout));

    holder.kill();
    QVERIFY(holder.waitForFinished(Timeout));
    QVERIFY(QFileInfo::exists(m_lockPath));

    QProcess next;
    QCOMPARE(hold(next), QString("held"));
    next.kill();
    QVERIFY(next.waitForFinished(Timeout));
}

void LockTest::refusedLaunchHandsItsMessageToTheHolder()
{
    QProcess holder;
    QCOMPARE(hold(holder), QString("held"));

    QProcess launch;
    QCOMPARE(hand(launch, "C:/Users/me/Tanglewood Ward 2026-11-08.wardupdate"), QString("handed"));
    QVERIFY(launch.waitForFinished(Timeout));
    QCOMPARE(launch.exitCode(), 0);

    QCOMPARE(nextLine(holder), QString("received C:/Users/me/Tanglewood Ward 2026-11-08.wardupdate"));
    QVERIFY(holder.waitForFinished(Timeout));
    QCOMPARE(holder.exitCode(), 0);
}
