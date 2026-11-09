#include "InstanceLock.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QThread>

namespace
{

constexpr int HandOverAttempts = 25;
constexpr int HandOverRetryMs = 200;
constexpr int HandOverTimeoutMs = 2000;

}  // namespace

InstanceLock::InstanceLock(const QString& path, QObject* parent)
    : QObject(parent)
    , m_lock(path)
{
    m_lock.setStaleLockTime(0);
    connect(&m_server, &QLocalServer::newConnection, this, &InstanceLock::onConnection);
}

bool InstanceLock::take()
{
    QDir().mkpath(QFileInfo(m_lock.fileName()).absolutePath());
    return m_lock.tryLock(0);
}

void InstanceLock::listen()
{
    // A socket file a dead holder left behind would refuse the name; holding the lock, this
    // process owns it.
    QLocalServer::removeServer(serverName());
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(serverName()))
    {
        qWarning("The instance lock could not listen for handovers: %s", qPrintable(m_server.errorString()));
    }
}

bool InstanceLock::handOver(const QString& message) const
{
    QLocalSocket socket;
    for (int attempt = 0; attempt < HandOverAttempts; ++attempt)
    {
        socket.connectToServer(serverName());
        if (socket.waitForConnected(HandOverTimeoutMs))
        {
            socket.write(message.toUtf8());
            socket.disconnectFromServer();
            return socket.state() == QLocalSocket::UnconnectedState
                || socket.waitForDisconnected(HandOverTimeoutMs);
        }
        QThread::msleep(HandOverRetryMs);
    }
    return false;
}

void InstanceLock::onConnection()
{
    while (QLocalSocket* socket = m_server.nextPendingConnection())
    {
        // The launch writes its message and disconnects, so the message is whole once it has.
        connect(socket, &QLocalSocket::disconnected, this,
                [this, socket]()
                {
                    emit handedOver(QString::fromUtf8(socket->readAll()));
                    socket->deleteLater();
                });
    }
}

QString InstanceLock::serverName() const
{
    QByteArray path = QFileInfo(m_lock.fileName()).absoluteFilePath().toUtf8();
    return "WardPlanning-" + QCryptographicHash::hash(path, QCryptographicHash::Sha256).toHex().left(32);
}
