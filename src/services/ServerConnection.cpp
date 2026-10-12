#include "ServerConnection.h"

#include <QPointer>

using namespace ServerContract;

namespace
{

/// The reply, called only while context is alive.
template<typename T>
Reply<T> guarded(QObject* context, Reply<T> reply)
{
    const QPointer<QObject> alive(context);
    return [alive, reply](const Result<T>& result) { if (alive) { reply(result); } };
}

} // namespace

ServerConnection::ServerConnection(ServerApi& server, const DeviceIdentity& identity)
    : m_server(server)
    , m_identity(identity)
{
}

ServerConnection::Writer ServerConnection::self() const
{
    return Writer{m_identity.keys(), m_identity.signing.signingKey};
}

Signer ServerConnection::sign(const Writer& writer, const QByteArray& bytes)
{
    return Signer{writer.keys, DocumentCrypto::sign(bytes, writer.signingKey)};
}

void ServerConnection::registerDocument(const Upload& upload, QObject* context, Reply<qint64> reply)
{
    m_server.registerDocument(sign(self(), signedBytes(CallRegister, upload.header.documentId, 0, upload)),
                              upload,
                              guarded(context, reply));
}

void ServerConnection::head(const QString& documentId, QObject* context, Reply<Head> reply)
{
    m_server.head(documentId, guarded(context, reply));
}

void ServerConnection::download(const QString& documentId, QObject* context, Reply<Snapshot> reply)
{
    m_server.download(documentId, guarded(context, reply));
}

void ServerConnection::upload(const QString& documentId,
                              qint64 baseVersion,
                              const Upload& upload,
                              QObject* context,
                              Reply<qint64> reply)
{
    uploadAs(self(), documentId, baseVersion, upload, context, reply);
}

void ServerConnection::uploadAs(const Writer& writer,
                                const QString& documentId,
                                qint64 baseVersion,
                                const Upload& upload,
                                QObject* context,
                                Reply<qint64> reply)
{
    m_server.upload(sign(writer, signedBytes(CallUpload, documentId, baseVersion, upload)),
                    documentId,
                    baseVersion,
                    upload,
                    guarded(context, reply));
}

void ServerConnection::takeLease(const QString& documentId, QObject* context, Reply<Lease> reply)
{
    m_server.takeLease(sign(self(), signedBytes(CallTakeLease, documentId)), documentId, guarded(context, reply));
}

void ServerConnection::releaseLease(const QString& documentId, QObject* context, Reply<Done> reply)
{
    m_server.releaseLease(sign(self(), signedBytes(CallReleaseLease, documentId)), documentId, guarded(context, reply));
}
