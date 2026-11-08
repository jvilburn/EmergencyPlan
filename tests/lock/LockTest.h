#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>

/// The instance lock, taken by copies of this executable, each its own process.
class LockTest : public QObject
{
    Q_OBJECT

public:
    explicit LockTest(QObject* parent);

private slots:
    void init();

    void secondProcessIsRefusedWhileTheFirstHoldsTheLock();
    void lockIsTakenOnceTheHolderIsKilled();

private:
    /// Starts copy holding the lock at m_lockPath, and returns the line it printed: held or
    /// refused.
    QString start(QProcess& copy) const;

    QTemporaryDir m_folder;
    QString m_lockPath;  ///< in a folder of its own for each test, which take() makes
};
