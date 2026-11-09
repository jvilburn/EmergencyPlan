#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

/// The instance lock, taken by copies of this executable, each its own process, and the
/// handover from a launch the lock refuses to the one holding it.
class LockTest : public QObject
{
    Q_OBJECT

public:
    explicit LockTest(QObject* parent);

private slots:
    void init();

    void secondProcessIsRefusedWhileTheFirstHoldsTheLock();
    void lockIsTakenOnceTheHolderIsKilled();
    void refusedLaunchHandsItsMessageToTheHolder();

private:
    /// Starts copy holding the lock at m_lockPath, and returns the line it printed: held or
    /// refused.
    QString hold(QProcess& copy) const;

    /// Starts copy handing message to the holder of the lock at m_lockPath, and returns the
    /// line it printed.
    QString hand(QProcess& copy, const QString& message) const;

    /// Starts copy with arguments, and returns the first line it printed.
    QString start(QProcess& copy, const QStringList& arguments) const;

    QTemporaryDir m_folder;
    QString m_lockPath;  ///< in a folder of its own for each test, which take() makes
};
