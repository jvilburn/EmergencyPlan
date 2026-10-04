#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QThread>

namespace
{

constexpr int HandOverMilliseconds = 3 * 1000;
constexpr int ConnectAttemptMilliseconds = 250;

} // namespace

SingleInstance::SingleInstance(QObject* parent)
    : QObject(parent)
{
}

SingleInstance::Claim SingleInstance::claim(const QStringList& arguments, QString* errorMessage)
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(directory);

    // Held by a live process only: a lock left by a crash is taken over, judged
    // by whether its owner still runs rather than by its age.
    m_lock = std::make_unique<QLockFile>(QDir(directory).filePath("instance.lock"));
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock(0))
    {
        if (m_lock->error() != QLockFile::LockFailedError)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = tr("Ward Planning could not create its lock file in %1.").arg(directory);
            }
            return Claim::Failed;
        }

        if (handOver(arguments))
        {
            return Claim::HandedOver;
        }

        if (errorMessage != nullptr)
        {
            *errorMessage = tr("Ward Planning is already running, but it did not answer. "
                               "Close it and try again.");
        }
        return Claim::Failed;
    }

    // A server left by a crash would refuse the name; the lock says it is ours.
    QLocalServer::removeServer(socketName());
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(socketName()))
    {
        // This instance runs; only later launches lose their hand-over.
        qWarning() << "Single instance server could not listen:" << m_server.errorString();
    }
    connect(&m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return Claim::Claimed;
}

bool SingleInstance::handOver(const QStringList& arguments)
{
    QElapsedTimer elapsed;
    elapsed.start();

    QLocalSocket socket;
    while (elapsed.elapsed() < HandOverMilliseconds)
    {
        socket.connectToServer(socketName());
        if (socket.waitForConnected(ConnectAttemptMilliseconds))
        {
            socket.write(QJsonDocument(QJsonArray::fromStringList(arguments)).toJson(QJsonDocument::Compact));
            const bool written = socket.waitForBytesWritten(HandOverMilliseconds);
            socket.disconnectFromServer();
            return written;
        }
        QThread::msleep(ConnectAttemptMilliseconds);
    }
    return false;
}

void SingleInstance::onNewConnection()
{
    QLocalSocket* socket = m_server.nextPendingConnection();
    if (socket == nullptr)
    {
        return;
    }

    // The launch writes its arguments and disconnects; they are complete then.
    m_received.insert(socket, QByteArray());
    connect(socket, &QLocalSocket::readyRead, this, &SingleInstance::onReadyRead);
    connect(socket, &QLocalSocket::disconnected, this, &SingleInstance::onHandedOver);
    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
}

void SingleInstance::onReadyRead()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket != nullptr)
    {
        m_received[socket].append(socket->readAll());
    }
}

void SingleInstance::onHandedOver()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket == nullptr)
    {
        return;
    }

    const QByteArray bytes = m_received.take(socket) + socket->readAll();
    QStringList arguments;
    for (const QJsonValue& value : QJsonDocument::fromJson(bytes).array())
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
