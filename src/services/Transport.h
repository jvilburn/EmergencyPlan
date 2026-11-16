#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

/// One request to the server: its method, its path from the server's address, its headers and
/// its body.
struct TransportRequest
{
    QByteArray method;
    QString path;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
};

/// The server's reply: its status and its body. A request that got no answer, because the
/// server could not be reached or the connection failed, has the status NoAnswer.
struct TransportReply
{
    static constexpr int NoAnswer = 0;

    int status = NoAnswer;
    QByteArray body;
};

/// One request on its way. It emits answered once, after Transport::send has returned, and
/// then deletes itself. Destroying it first drops the answer.
class TransportCall : public QObject
{
    Q_OBJECT

public:
    explicit TransportCall(QObject* parent) : QObject(parent) {}

signals:
    void answered(const TransportReply& reply);
};

/// An open WebSocket to the server.
class TransportChannel : public QObject
{
    Q_OBJECT

public:
    explicit TransportChannel(QObject* parent) : QObject(parent) {}

    /// Closes the socket now. closed is not emitted for a close asked for here.
    virtual void close() = 0;

signals:
    /// The server sent a text message.
    void messageReceived(const QByteArray& message);

    /// The socket closed, or never opened. Emitted once, after Transport::open has returned.
    void closed();
};

/// What carries requests to the server and replies back: HTTP in the app, and the fake server,
/// in process, in the tests.
class Transport
{
public:
    virtual ~Transport() = default;

    /// Sends request. The call is a child of parent.
    virtual TransportCall* send(const TransportRequest& request, QObject* parent) = 0;

    /// Opens a WebSocket with request, a GET whose headers ride on the upgrade. The channel is
    /// a child of parent.
    virtual TransportChannel* open(const TransportRequest& request, QObject* parent) = 0;
};
