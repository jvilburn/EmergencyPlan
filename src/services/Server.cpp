#include "Server.h"

#include "Base64Url.h"
#include "Crypto.h"
#include "SignedLine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QString pathOf(const DocumentId& document)
{
    return "/documents/" + document.toString();
}

/// The Ward-Signer and Ward-Signature headers of signer signing line.
QList<QPair<QByteArray, QByteArray>> signedBy(const Identity& signer, const QByteArray& line)
{
    QByteArray keys = Base64Url::encode(signer.publicKey()).toLatin1() + '.'
        + Base64Url::encode(signer.verifyKey()).toLatin1();
    return {{"Ward-Signer", keys}, {"Ward-Signature", Base64Url::encode(signer.sign(line)).toLatin1()}};
}
}  // namespace

ServerWrite::ServerWrite(TransportCall* call, QObject* parent)
    : QObject(parent)
{
    call->setParent(this);
    connect(call, &TransportCall::answered, this, &ServerWrite::onAnswered);
}

void ServerWrite::onAnswered(const TransportReply& reply)
{
    emit answered(reply.status == 200 || reply.status == 201);
    deleteLater();
}

ServerReading::ServerReading(TransportCall* call, QObject* parent)
    : QObject(parent)
{
    call->setParent(this);
    connect(call, &TransportCall::answered, this, &ServerReading::onAnswered);
}

void ServerReading::onAnswered(const TransportReply& reply)
{
    switch (reply.status)
    {
    case 200:
        emit answered(ServerRead{ServerReadStatus::Found, reply.body});
        break;
    case 404:
        emit answered(ServerRead{ServerReadStatus::Missing, QByteArray()});
        break;
    default:
        emit answered(ServerRead{ServerReadStatus::NoAnswer, QByteArray()});
        break;
    }
    deleteLater();
}

ServerChannel::ServerChannel(TransportChannel* channel, QObject* parent)
    : QObject(parent)
    , m_channel(channel)
{
    channel->setParent(this);
    connect(channel, &TransportChannel::messageReceived, this, &ServerChannel::onMessageReceived);
    connect(channel, &TransportChannel::closed, this, &ServerChannel::closed);
}

void ServerChannel::onMessageReceived(const QByteArray& message)
{
    QJsonObject json = QJsonDocument::fromJson(message).object();
    QSet<QByteArray> present;
    const QJsonArray fingerprints = json.value("present").toArray();
    for (const QJsonValue& text : fingerprints)
    {
        std::optional<QByteArray> fingerprint = Base64Url::decode(text.toString(), Crypto::FingerprintBytes);
        if (fingerprint.has_value())
        {
            present.insert(*fingerprint);
        }
    }
    emit told(present, json.value("version").toInteger());
}

Server::Server(Transport* transport)
    : m_transport(transport)
{
}

ServerWrite* Server::create(const DocumentId& document, const QByteArray& version, const Identity& signer,
                            QObject* parent)
{
    return write("POST", "/documents", SignedLine::ofVersion(SignedCall::Create, document, version), version,
                 signer, parent);
}

ServerWrite* Server::push(const DocumentId& document, const QByteArray& version, const Identity& signer,
                          QObject* parent)
{
    return write("PUT", pathOf(document), SignedLine::ofVersion(SignedCall::Push, document, version), version,
                 signer, parent);
}

ServerReading* Server::head(const DocumentId& document, QObject* parent)
{
    TransportRequest request{"GET", pathOf(document) + "/head", {}, QByteArray()};
    return new ServerReading(m_transport->send(request, nullptr), parent);
}

ServerReading* Server::fetch(const DocumentId& document, QObject* parent)
{
    TransportRequest request{"GET", pathOf(document), {}, QByteArray()};
    return new ServerReading(m_transport->send(request, nullptr), parent);
}

ServerReading* Server::releases(QObject* parent)
{
    TransportRequest request{"GET", "/releases/latest", {}, QByteArray()};
    return new ServerReading(m_transport->send(request, nullptr), parent);
}

ServerChannel* Server::openChannel(const DocumentId& document, const Identity& signer, QObject* parent)
{
    TransportRequest request{"GET", pathOf(document) + "/channel",
                             signedBy(signer, SignedLine::ofChannel(document)), QByteArray()};
    return new ServerChannel(m_transport->open(request, nullptr), parent);
}

ServerWrite* Server::write(const QByteArray& method, const QString& path, const QByteArray& line,
                           const QByteArray& version, const Identity& signer, QObject* parent)
{
    TransportRequest request{method, path, signedBy(signer, line), version};
    return new ServerWrite(m_transport->send(request, nullptr), parent);
}
