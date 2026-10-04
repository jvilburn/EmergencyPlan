#pragma once

#include <QLockFile>
#include <QLocalServer>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

/// One running app per user. The settings file, documents, base copies, and lease
/// heartbeat all assume a single writer.
///
/// The first instance holds a lock file in AppDataLocation and listens on a local
/// socket only the same user can reach. A later launch finds the lock held, hands
/// its command-line arguments to the running instance - so a double-clicked
/// .wardplanning file opens there - and exits.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject* parent);

    /// True when this process is now the one instance. Otherwise arguments have
    /// gone to the running one, and this process should exit.
    bool claim(const QStringList& arguments);

signals:
    /// A later launch's arguments, handed over by it.
    void argumentsReceived(const QStringList& arguments);

private:
    void onNewConnection();
    void onHandedOver();

    /// Named for this user's app data, so two users on one machine each have
    /// their own.
    static QString socketName();

    std::unique_ptr<QLockFile> m_lock;
    QLocalServer m_server;
};
