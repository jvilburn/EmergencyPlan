#pragma once

#include "ServerContract.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class AppSettings;

/// The server contract over HTTPS, as HttpWire.h describes it.
///
/// The base address is AppSettings::serverAddress(), read on every call, so a change
/// takes effect at once. With none set every call answers Unreachable, and the app
/// works from its local copies.
///
/// Replies arrive on a later turn of the event loop, never before the call returns.
/// Nothing a request carries is logged.
class HttpServerApi : public QObject, public ServerContract::ServerApi
{
    Q_OBJECT

public:
    static constexpr int TransferTimeoutMilliseconds = 30 * 1000;

    HttpServerApi(const AppSettings& settings, QObject* parent);

    void registerDocument(const ServerContract::Signer& signer,
                          const ServerContract::Upload& upload,
                          ServerContract::Reply<qint64> reply) override;
    void head(const QString& documentId, ServerContract::Reply<ServerContract::Head> reply) override;
    void download(const QString& documentId, ServerContract::Reply<ServerContract::Snapshot> reply) override;
    void upload(const ServerContract::Signer& signer,
                const QString& documentId,
                qint64 baseVersion,
                const ServerContract::Upload& upload,
                ServerContract::Reply<qint64> reply) override;
    void takeLease(const ServerContract::Signer& signer,
                   const QString& documentId,
                   ServerContract::Reply<ServerContract::Lease> reply) override;
    void releaseLease(const ServerContract::Signer& signer,
                      const QString& documentId,
                      ServerContract::Reply<ServerContract::Done> reply) override;
    void advanceClock(qint64 seconds, ServerContract::Reply<ServerContract::Done> reply) override;

private:
    /// One request: its method and path below the base URL, an optional JSON body,
    /// and the signer when the call is signed.
    struct Call
    {
        QByteArray method;
        QString path;
        std::optional<QJsonObject> body;
        std::optional<ServerContract::Signer> signer;
    };

    /// Sends call and answers reply with read's reading of a 2xx body, or the
    /// error the answer names.
    template<typename T>
    void send(const Call& call,
              std::function<std::optional<T>(const QJsonObject&)> read,
              ServerContract::Reply<T> reply);

    /// An answer that carries nothing but success.
    void sendForDone(const Call& call, ServerContract::Reply<ServerContract::Done> reply);

    const AppSettings& m_settings;
    QNetworkAccessManager m_network;
};
