#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QStandardPaths>

namespace
{

constexpr int HandOverMilliseconds = 3 * 1000;

} // namespace

SingleInstance::SingleInstance(QObject* parent)
    : QObject(parent)
{
}

bool SingleInstance::claim(const QStringList& arguments)
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(directory);

    // Held by a live process only: a lock left by a crash is taken over, judged
    // by whether its owner still runs rather than by its age.
    m_lock = std::make_unique<QLockFile>(QDir(directory).filePath("instance.lock"));
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock(0))
    {
        QLocalSocket socket;
        socket.connectToServer(socketName());
        if (socket.waitForConnected(HandOverMilliseconds))
        {
            socket.write(QJsonDocument(QJsonArray::fromStringList(arguments)).toJson(QJsonDocument::Compact));
            socket.waitForBytesWritten(HandOverMilliseconds);
            socket.disconnectFromServer();
        }
        return false;
    }

    // A server left by a crash would refuse the name; the lock says it is ours.
    QLocalServer::removeServer(socketName());
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.listen(socketName());
    connect(&m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

void SingleInstance::onNewConnection()
{
    QLocalSocket* socket = m_server.nextPendingConnection();
    if (socket == nullptr)
    {
        return;
    }

    // The launch writes its arguments and disconnects; they are all here then.
    connect(socket, &QLocalSocket::disconnected, this, &SingleInstance::onHandedOver);
    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
}

void SingleInstance::onHandedOver()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket == nullptr)
    {
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(socket->readAll());
    QStringList arguments;
    for (const QJsonValue& value : json.array())
    {
        arguments.append(value.toString());
    }
    emit argumentsReceived(arguments);
}

QString SingleInstance::socketName()
{
    const QByteArray place = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toUtf8();
    return QString("WardPlanning-") + QCryptographicHash::hash(place, QCryptographicHash::Sha256).toHex().left(16);
}
