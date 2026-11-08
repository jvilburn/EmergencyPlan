#include "LockTest.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTest>

namespace
{

constexpr int Timeout = 30000;

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

QString LockTest::start(QProcess& copy) const
{
    copy.start(QCoreApplication::applicationFilePath(), {"hold", m_lockPath});
    while (!copy.canReadLine() && copy.waitForReadyRead(Timeout))
    {
    }
    return QString::fromUtf8(copy.readLine()).trimmed();
}

void LockTest::secondProcessIsRefusedWhileTheFirstHoldsTheLock()
{
    QProcess holder;
    QCOMPARE(start(holder), QString("held"));

    QProcess second;
    QCOMPARE(start(second), QString("refused"));
    QVERIFY(second.waitForFinished(Timeout));
    QCOMPARE(second.exitCode(), 1);

    holder.closeWriteChannel();
    QVERIFY(holder.waitForFinished(Timeout));
    QCOMPARE(holder.exitCode(), 0);
}

void LockTest::lockIsTakenOnceTheHolderIsKilled()
{
    QProcess holder;
    QCOMPARE(start(holder), QString("held"));
    QProcess refused;
    QCOMPARE(start(refused), QString("refused"));
    QVERIFY(refused.waitForFinished(Timeout));

    holder.kill();
    QVERIFY(holder.waitForFinished(Timeout));
    QVERIFY(QFileInfo::exists(m_lockPath));

    QProcess next;
    QCOMPARE(start(next), QString("held"));
    next.closeWriteChannel();
    QVERIFY(next.waitForFinished(Timeout));
    QCOMPARE(next.exitCode(), 0);
}
