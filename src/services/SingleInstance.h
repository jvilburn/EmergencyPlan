#pragma once

#include <QByteArray>
#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

/// One running app per user. The settings file, documents, base copies, and lease
/// heartbeat all assume a single writer.
///
/// The first instance holds a lock file in local app data and listens on a local
/// socket only the same user can reach. A later launch finds the lock held, hands
/// its command-line arguments to the running instance - so a double-clicked
/// .wardplanning file opens there - and exits.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    enum class Claim
    {
        Claimed,        ///< this process is the one instance
        HandedOver,     ///< the running instance has the arguments; exit
        Failed          ///< neither: the reason is in errorMessage
    };

    explicit SingleInstance(QObject* parent);

    Claim claim(const QStringList& arguments, QString* errorMessage);

signals:
    /// A later launch's arguments, handed over by it.
    void argumentsReceived(const QStringList& arguments);

private:
    void onNewConnection();
    void onReadyRead();
    void onHandedOver();

    /// Gives the running instance the arguments, allowing it a moment to finish
    /// starting if it has only just taken the lock.
    static bool handOver(const QStringList& arguments);

    /// Named for this user's app data, so two users on one machine each have
    /// their own.
    static QString socketName();

    std::unique_ptr<QLockFile> m_lock;
    QLocalServer m_server;
    QHash<QLocalSocket*, QByteArray> m_received;
};
