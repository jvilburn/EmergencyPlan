#pragma once

#include "ServerContract.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

#include <functional>
#include <optional>

class AppSettings;

/// ServerApi over HTTP(S), in HttpWire's format. Plain http is for a development
/// server on this machine; anywhere else the address is https. Transport only:
/// routes, bodies, the session token in a header, and HTTP status mapped onto
/// ServerError. Signing in is ServerConnection's.
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

    void registerPerson(const ServerContract::Session& session,
                        const ServerContract::Registration& registration,
                        ServerContract::Reply<ServerContract::Done> reply) override;
    void me(const ServerContract::Session& session, ServerContract::Reply<ServerContract::Me> reply) override;
    void unlinkDevice(const ServerContract::Session& session,
                      const Fingerprint& device,
                      ServerContract::Reply<ServerContract::Done> reply) override;

    void startLink(const ServerContract::Session& session,
                   const PublicKey& publicKey,
                   const QString& deviceName,
                   ServerContract::Reply<QString> reply) override;
    void link(const ServerContract::Session& session,
              const QString& code,
              ServerContract::Reply<ServerContract::LinkState> reply) override;
    void approveLink(const ServerContract::Session& session,
                     const QString& code,
                     ServerContract::Reply<ServerContract::Done> reply) override;

    void registerDocument(const ServerContract::Session& session,
                          const ServerContract::Upload& upload,
                          ServerContract::Reply<qint64> reply) override;
    void head(const ServerContract::Session& session,
              const QString& documentId,
              ServerContract::Reply<ServerContract::Head> reply) override;
    void download(const ServerContract::Session& session,
                  const QString& documentId,
                  ServerContract::Reply<ServerContract::Snapshot> reply) override;
    void upload(const ServerContract::Session& session,
                const QString& documentId,
                qint64 baseVersion,
                const ServerContract::Upload& upload,
                ServerContract::Reply<qint64> reply) override;

    void takeLease(const ServerContract::Session& session,
                   const QString& documentId,
                   ServerContract::Reply<ServerContract::Lease> reply) override;
    void releaseLease(const ServerContract::Session& session,
                      const QString& documentId,
                      ServerContract::Reply<ServerContract::Done> reply) override;

    void history(const ServerContract::Session& session,
                 const QString& documentId,
                 qint64 after,
                 ServerContract::Reply<QList<ServerContract::HistoryEntry>> reply) override;
    void requestAccess(const ServerContract::Session& session,
                       const QString& documentId,
                       ServerContract::Reply<ServerContract::Done> reply) override;
    void accessRequests(const ServerContract::Session& session,
                        const QString& documentId,
                        ServerContract::Reply<QList<ServerContract::AccessRequest>> reply) override;
    void declineRequest(const ServerContract::Session& session,
                        const QString& documentId,
                        qint64 index,
                        ServerContract::Reply<ServerContract::Done> reply) override;
    void myRequestStatus(const ServerContract::Session& session,
                         const QString& documentId,
                         ServerContract::Reply<ServerContract::RequestStatus> reply) override;
    void grant(const ServerContract::Session& session,
               const QString& documentId,
               qint64 index,
               ServerContract::Reply<ServerContract::Done> reply) override;
    void removeMember(const ServerContract::Session& session,
                      const QString& documentId,
                      const MemberId& member,
                      ServerContract::Reply<ServerContract::Done> reply) override;
    void setRole(const ServerContract::Session& session,
                 const QString& documentId,
                 const MemberId& member,
                 ServerContract::Role role,
                 ServerContract::Reply<ServerContract::Done> reply) override;

    void advanceClock(qint64 seconds, ServerContract::Reply<ServerContract::Done> reply) override;

private:
    /// One request: its method and path below the base URL, an optional JSON body,
    /// and the session token when the call carries one.
    struct Call
    {
        QByteArray method;
        QString path;
        std::optional<QJsonObject> body;
        QString token;
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
