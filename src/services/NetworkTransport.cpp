#include "NetworkTransport.h"

NetworkCall::NetworkCall(QNetworkReply* reply, QObject* parent)
    : TransportCall(parent)
    , m_reply(reply)
{
    reply->setParent(this);
    connect(reply, &QNetworkReply::finished, this, &NetworkCall::onFinished);
}

void NetworkCall::onFinished()
{
    // A reply with no HTTP status, as from a connection that failed or timed out, reads as 0,
    // which is NoAnswer.
    int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    emit answered(TransportReply{status, m_reply->readAll()});
    deleteLater();
}

NetworkChannel::NetworkChannel(const QNetworkRequest& request, QObject* parent)
    : TransportChannel(parent)
{
    m_pingTimer.setInterval(PingMilliseconds);
    connect(&m_pingTimer, &QTimer::timeout, this, &NetworkChannel::onPingDue);
    connect(&m_socket, &QWebSocket::connected, this, &NetworkChannel::onConnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &NetworkChannel::onTextMessageReceived);
    connect(&m_socket, &QWebSocket::pong, this, &NetworkChannel::onPong);
    connect(&m_socket, &QWebSocket::disconnected, this, &NetworkChannel::onGone);
    connect(&m_socket, &QWebSocket::errorOccurred, this, &NetworkChannel::onGone);
    m_socket.open(request);
}

void NetworkChannel::close()
{
    m_gone = true;
    m_pingTimer.stop();
    m_socket.close();
}

void NetworkChannel::onConnected()
{
    m_pingTimer.start();
}

void NetworkChannel::onTextMessageReceived(const QString& message)
{
    emit messageReceived(message.toUtf8());
}

void NetworkChannel::onPong()
{
    m_awaitingPong = false;
}

void NetworkChannel::onPingDue()
{
    if (m_awaitingPong)
    {
        m_socket.abort();
        onGone();
        return;
    }
    m_awaitingPong = true;
    m_socket.ping();
}

void NetworkChannel::onGone()
{
    if (m_gone)
    {
        return;
    }
    m_gone = true;
    m_pingTimer.stop();
    emit closed();
}

NetworkTransport::NetworkTransport(const QString& address, QObject* parent)
    : QObject(parent)
    , m_address(address)
{
}

TransportCall* NetworkTransport::send(const TransportRequest& request, QObject* parent)
{
    QNetworkRequest networked = networkRequest(request, QUrl(m_address + request.path));
    networked.setTransferTimeout(TransferTimeoutMilliseconds);
    if (!request.body.isEmpty())
    {
        networked.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    }
    return new NetworkCall(m_network.sendCustomRequest(networked, request.method, request.body), parent);
}

TransportChannel* NetworkTransport::open(const TransportRequest& request, QObject* parent)
{
    QUrl url(m_address + request.path);
    url.setScheme(url.scheme() == "http" ? "ws" : "wss");
    return new NetworkChannel(networkRequest(request, url), parent);
}

QNetworkRequest NetworkTransport::networkRequest(const TransportRequest& request, const QUrl& url)
{
    QNetworkRequest networked(url);
    for (const QPair<QByteArray, QByteArray>& header : request.headers)
    {
        networked.setRawHeader(header.first, header.second);
    }
    return networked;
}
