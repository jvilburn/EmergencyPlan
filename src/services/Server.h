#pragma once

#include "Id.h"
#include "Identity.h"
#include "Transport.h"

#include <QByteArray>
#include <QObject>
#include <QSet>

enum class ServerReadStatus
{
    Found,
    Missing,  ///< the server has no such ward
    NoAnswer  ///< the server could not be reached, or answered with a fault
};

/// What a head or a fetch answered.
struct ServerRead
{
    ServerReadStatus status;
    QByteArray bytes;  ///< when Found: the header line for a head, the whole version for a fetch
};

/// A create or a push on its way. It emits answered once and then deletes itself; destroying
/// it first drops the answer.
class ServerWrite : public QObject
{
    Q_OBJECT

public:
    /// Reads call's reply; call becomes this object's child.
    ServerWrite(TransportCall* call, QObject* parent);

signals:
    /// Whether the server stored the version. Any other answer, or none, is not stored.
    void answered(bool stored);

private slots:
    void onAnswered(const TransportReply& reply);
};

/// A head or a fetch on its way, as ServerWrite is.
class ServerReading : public QObject
{
    Q_OBJECT

public:
    ServerReading(TransportCall* call, QObject* parent);

signals:
    void answered(const ServerRead& read);

private slots:
    void onAnswered(const TransportReply& reply);
};

/// A ward's live channel: the server's messages on it, read.
class ServerChannel : public QObject
{
    Q_OBJECT

public:
    /// Reads channel's messages; channel becomes this object's child.
    ServerChannel(TransportChannel* channel, QObject* parent);

    /// Closes the channel now. closed is not emitted for a close asked for here.
    void close() { m_channel->close(); }

signals:
    /// The server said who is here, each by fingerprint, this device among them, and the
    /// number of the version it holds.
    void told(const QSet<QByteArray>& present, qint64 version);

    /// The channel closed, or never opened.
    void closed();

private slots:
    void onMessageReceived(const QByteArray& message);

private:
    TransportChannel* m_channel;
};

/// The Ward Planning server's calls, as docs/SHARED_WARD_DESIGN.md gives them under What the
/// server is and The live channel: each becomes one request on the transport, signed where the
/// server asks for a signature, and its reply is read down to what the app acts on. What each
/// call returns is a child of parent.
class Server
{
public:
    /// transport carries the requests; the server does not own it.
    explicit Server(Transport* transport);

    /// POST /documents: version becomes the ward's first version on the server. signer is the
    /// version's writer.
    ServerWrite* create(const DocumentId& document, const QByteArray& version, const Identity& signer,
                        QObject* parent);

    /// PUT /documents/{id}: version becomes the ward's current version, when the server keeps
    /// it. signer is the version's writer.
    ServerWrite* push(const DocumentId& document, const QByteArray& version, const Identity& signer,
                      QObject* parent);

    /// GET /documents/{id}/head: the current version's header line.
    ServerReading* head(const DocumentId& document, QObject* parent);

    /// GET /documents/{id}: the current version.
    ServerReading* fetch(const DocumentId& document, QObject* parent);

    /// Opens document's live channel, signed by signer.
    ServerChannel* openChannel(const DocumentId& document, const Identity& signer, QObject* parent);

private:
    ServerWrite* write(const QByteArray& method, const QString& path, const QByteArray& line,
                       const QByteArray& version, const Identity& signer, QObject* parent);

    Transport* m_transport;
};
