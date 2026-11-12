#pragma once

#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <QString>

/// The per-user lock that keeps one Ward Planning running, so two processes never write the
/// same ward file and journal. Only a dead holder makes it stale, never its age. The holder
/// listens on a local socket named for the lock's path, so only launches of the same user
/// reach it, and a launch the lock refuses hands what it was opened with to the holder.
class InstanceLock : public QObject
{
    Q_OBJECT

public:
    InstanceLock(const QString& path, QObject* parent);

    /// Takes the lock, making its folder when there is none. Answers false when another process
    /// holds the lock or the lock cannot be taken.
    bool take();

    /// Starts taking handovers. The holder calls it once it can act on one.
    void listen();

    /// Hands message to the process holding the lock: the update or invitation file this
    /// launch was opened with, or empty for none. Waits a few seconds for a holder still starting up; false when
    /// none took the message.
    bool handOver(const QString& message) const;

signals:
    /// A launch the lock refused handed over message.
    void handedOver(const QString& message);

private slots:
    void onConnection();

private:
    QString serverName() const;

    QLockFile m_lock;
    QLocalServer m_server;
};
