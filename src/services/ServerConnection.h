#pragma once

#include "DeviceIdentity.h"
#include "ServerContract.h"

#include <QObject>
#include <QString>

/// One device's way to the server. Reads go as they are; writes are signed, by this
/// device unless a call says otherwise, and the server checks the signer against the
/// document's current header.
///
/// Every reply is delivered only while its context is alive, as with
/// QTimer::singleShot: a session destroyed with a call outstanding never hears the
/// answer. Request bodies stay in memory only; they are ciphertext, and keeping them
/// out of logs keeps it that way.
class ServerConnection
{
public:
    ServerConnection(ServerContract::ServerApi& server, const DeviceIdentity& identity);

    ServerConnection(const ServerConnection&) = delete;
    ServerConnection& operator=(const ServerConnection&) = delete;

    /// Someone who may sign a write: a device, or an invite whose seed this device
    /// holds while it joins.
    struct Writer
    {
        DeviceKeys keys;
        SigningKey signingKey;
    };

    /// This device as a writer.
    Writer self() const;

    void registerDocument(const ServerContract::Upload& upload, QObject* context, ServerContract::Reply<qint64> reply);
    void head(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Head> reply);
    void download(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Snapshot> reply);
    void upload(const QString& documentId,
                qint64 baseVersion,
                const ServerContract::Upload& upload,
                QObject* context,
                ServerContract::Reply<qint64> reply);

    /// An upload signed by writer rather than this device: a joining device signs its
    /// first version as the invite it holds.
    void uploadAs(const Writer& writer,
                  const QString& documentId,
                  qint64 baseVersion,
                  const ServerContract::Upload& upload,
                  QObject* context,
                  ServerContract::Reply<qint64> reply);

    void takeLease(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Lease> reply);
    void releaseLease(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Done> reply);

private:
    static ServerContract::Signer sign(const Writer& writer, const QByteArray& bytes);

    ServerContract::ServerApi& m_server;
    DeviceIdentity m_identity;
};
