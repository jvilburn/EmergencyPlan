#pragma once

#include "Transport.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

/// One HTTPS request on its way. Destroying it abandons the request.
class NetworkCall : public TransportCall
{
    Q_OBJECT

public:
    /// Answers with reply's status and body once it finishes; reply becomes this call's child.
    NetworkCall(QNetworkReply* reply, QObject* parent);

private slots:
    void onFinished();

private:
    QNetworkReply* m_reply;
};

/// A WebSocket to the server. It pings every 30 seconds, and a pong that has not arrived when
/// the next ping is due closes it, as a connection that died silently would otherwise look
/// open. An open that has not connected when the first ping is due is closed the same way.
class NetworkChannel : public TransportChannel
{
    Q_OBJECT

public:
    /// Opens the socket with request, whose headers ride on the upgrade.
    NetworkChannel(const QNetworkRequest& request, QObject* parent);

    void close() override;

private slots:
    void onConnected();
    void onTextMessageReceived(const QString& message);
    void onPong();
    void onPingDue();

    /// The socket closed or failed: closed is emitted, once, unless this side closed it.
    void onGone();

private:
    QTimer m_pingTimer;   ///< declared before the socket, whose destruction can still report a close
    QWebSocket m_socket;
    bool m_awaitingPong = false;  ///< a ping, or the open itself, has not been answered yet
    bool m_gone = false;  ///< closed has been emitted, or this side closed the socket

    static constexpr int PingMilliseconds = 30000;
};

/// The app's transport: HTTPS and WebSocket to the Ward Planning server.
class NetworkTransport : public QObject, public Transport
{
    Q_OBJECT

public:
    /// The server the app is built to use.
    static constexpr char BuiltInAddress[] = "https://ward-planning-dev.ward-planning-server.workers.dev";

    /// Sends to the server at address, such as BuiltInAddress, which has no final slash.
    NetworkTransport(const QString& address, QObject* parent);

    TransportCall* send(const TransportRequest& request, QObject* parent) override;
    TransportChannel* open(const TransportRequest& request, QObject* parent) override;

private:
    /// request as a network request to url, with its headers.
    static QNetworkRequest networkRequest(const TransportRequest& request, const QUrl& url);

    QString m_address;
    QNetworkAccessManager m_network;

    static constexpr int TransferTimeoutMilliseconds = 30000;
};
