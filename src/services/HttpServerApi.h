#pragma once

#include "ServerContract.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

#include <functional>
#include <optional>

class AppSettings;

/// ServerApi over HTTPS, in HttpWire's format. Transport only: routes, bodies, the
/// session token and request signature in headers, and HTTP status mapped onto
/// ServerError. Signing in and signing requests are ServerConnection's.
///
/// The base URL is AppSettings::serverAddress(), read on every request, so an
/// override takes effect without restarting; with none set, every call is
/// Unreachable. Every reply comes from the network's own signal, or the event loop
/// for a call that never leaves, so none arrives before its call returns.
///
/// Nothing here logs a token, a challenge, a signature, or a body.
class HttpServerApi : public QObject, public ServerContract::ServerApi
{
    Q_OBJECT

public:
    /// Long enough for a large blob on a slow connection.
    static constexpr int TransferTimeoutMilliseconds = 60 * 1000;

    HttpServerApi(const AppSettings& settings, QObject* parent);

    void challenge(ServerContract::Reply<QByteArray> reply) override;
    void authenticate(const VerifyKey& device,
                      const QByteArray& challenge,
                      const QByteArray& signature,
                      ServerContract::Reply<ServerContract::Session> reply) override;

    void registerDocument(const ServerContract::Session& session,
                          const ServerContract::Upload& upload,
                          const QByteArray& requestSignature,
                          ServerContract::Reply<qint64> reply) override;
    void head(const ServerContract::Session& session,
              const QString& documentId,
              ServerContract::Reply<ServerContract::Head> reply) override;
    void headers(const ServerContract::Session& session,
                 const QString& documentId,
                 qint64 after,
                 ServerContract::Reply<ServerContract::HeaderChain> reply) override;
    void download(const ServerContract::Session& session,
                  const QString& documentId,
                  qint64 after,
                  ServerContract::Reply<ServerContract::Snapshot> reply) override;
    void upload(const ServerContract::Session& session,
                const QString& documentId,
                qint64 baseVersion,
                const ServerContract::Upload& upload,
                const QByteArray& requestSignature,
                ServerContract::Reply<qint64> reply) override;

    void takeLease(const ServerContract::Session& session,
                   const QString& documentId,
                   const QByteArray& requestSignature,
                   ServerContract::Reply<ServerContract::Lease> reply) override;
    void releaseLease(const ServerContract::Session& session,
                      const QString& documentId,
                      const QByteArray& requestSignature,
                      ServerContract::Reply<ServerContract::Done> reply) override;

    void requestAccess(const ServerContract::Session& session,
                       const QString& documentId,
                       const ServerContract::AccessRequest& request,
                       const QByteArray& requestSignature,
                       ServerContract::Reply<ServerContract::Done> reply) override;
    void accessRequests(const ServerContract::Session& session,
                        const QString& documentId,
                        ServerContract::Reply<QList<ServerContract::AccessRequest>> reply) override;
    void clearAccessRequest(const ServerContract::Session& session,
                            const QString& documentId,
                            const Fingerprint& device,
                            const QByteArray& requestSignature,
                            ServerContract::Reply<ServerContract::Done> reply) override;
    void myRequestStatus(const ServerContract::Session& session,
                         const QString& documentId,
                         ServerContract::Reply<ServerContract::RequestStatus> reply) override;

    void myDocuments(const ServerContract::Session& session, ServerContract::Reply<QStringList> reply) override;

    void startPairing(const ServerContract::Session& session,
                      const QString& deviceName,
                      const QByteArray& commitment,
                      const QByteArray& requestSignature,
                      ServerContract::Reply<QString> reply) override;
    void pairing(const ServerContract::Session& session,
                 const QString& code,
                 ServerContract::Reply<ServerContract::PairingState> reply) override;
    void respondToPairing(const ServerContract::Session& session,
                          const QString& code,
                          const DeviceKeys& responder,
                          const QByteArray& requestSignature,
                          ServerContract::Reply<ServerContract::Done> reply) override;
    void revealPairing(const ServerContract::Session& session,
                       const QString& code,
                       const DeviceKeys& initiator,
                       const QByteArray& nonce,
                       const QByteArray& requestSignature,
                       ServerContract::Reply<ServerContract::Done> reply) override;
    void handOver(const ServerContract::Session& session,
                  const QString& code,
                  const QByteArray& handover,
                  const QByteArray& requestSignature,
                  ServerContract::Reply<ServerContract::Done> reply) override;

    void advanceClock(qint64 seconds, ServerContract::Reply<ServerContract::Done> reply) override;

private:
    /// One request: its method and path below the base URL, an optional JSON body,
    /// and the session token and request signature when the call carries them.
    struct Call
    {
        QByteArray method;
        QString path;
        std::optional<QJsonObject> body;
        QString token;
        QByteArray requestSignature;
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
