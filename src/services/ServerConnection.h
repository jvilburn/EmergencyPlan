#pragma once

#include "DeviceIdentity.h"
#include "ServerContract.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

/// One device's way to the server. It holds the session token, signs in on first
/// use - fetch a challenge, sign it under SignatureTag::Auth, exchange it for a
/// token - and signs every mutating request under SignatureTag::Request. Everything
/// above it calls the server without seeing sessions or signatures.
///
/// An expired session is signed into again once: on Unauthenticated the connection
/// signs in afresh and repeats the call, and a second Unauthenticated is the
/// answer. Calls made while a sign-in is in flight wait for it, so a burst of calls
/// costs one sign-in.
///
/// Every reply is delivered only while its context is alive, as with
/// QTimer::singleShot: a session destroyed with a call outstanding never hears the
/// answer.
///
/// Tokens, challenge responses, and request bodies stay in memory only; the
/// bodies are ciphertext, and keeping them out of logs keeps it that way.
class ServerConnection
{
public:
    ServerConnection(ServerContract::ServerApi& server, const DeviceIdentity& identity);

    ServerConnection(const ServerConnection&) = delete;
    ServerConnection& operator=(const ServerConnection&) = delete;

    void registerDocument(const ServerContract::Upload& upload,
                          QObject* context,
                          ServerContract::Reply<qint64> reply);
    void head(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Head> reply);
    void download(const QString& documentId,
                  qint64 after,
                  QObject* context,
                  ServerContract::Reply<ServerContract::Snapshot> reply);
    void headers(const QString& documentId,
                 qint64 after,
                 QObject* context,
                 ServerContract::Reply<ServerContract::HeaderChain> reply);
    void upload(const QString& documentId,
                qint64 baseVersion,
                const ServerContract::Upload& upload,
                QObject* context,
                ServerContract::Reply<qint64> reply);
    void takeLease(const QString& documentId,
                   QObject* context,
                   ServerContract::Reply<ServerContract::Lease> reply);
    void releaseLease(const QString& documentId,
                      QObject* context,
                      ServerContract::Reply<ServerContract::Done> reply);
    void myDocuments(QObject* context, ServerContract::Reply<QStringList> reply);

private:
    /// Makes one call with a session, answering through the reply it is given.
    template<typename T>
    using Attempt = std::function<void(const ServerContract::Session&, ServerContract::Reply<T>)>;

    using SessionReady = std::function<void(const ServerContract::Result<ServerContract::Session>&)>;

    template<typename T>
    void call(QObject* context, Attempt<T> attempt, ServerContract::Reply<T> reply);

    template<typename T>
    void attemptWith(const ServerContract::Result<ServerContract::Session>& session,
                     Attempt<T> attempt,
                     ServerContract::Reply<T> reply,
                     bool mayRetry);

    template<typename T>
    void answered(const QString& token,
                  const ServerContract::Result<T>& result,
                  Attempt<T> attempt,
                  ServerContract::Reply<T> reply,
                  bool mayRetry);

    // --- Signing in ---

    void withSession(SessionReady ready);
    void onChallenge(const ServerContract::Result<QByteArray>& challenge);
    void finishSignIn(const ServerContract::Result<ServerContract::Session>& session);

    // --- Each endpoint, signed with the session it is given ---

    void sendRegisterDocument(const ServerContract::Session& session,
                              const ServerContract::Upload& upload,
                              ServerContract::Reply<qint64> reply);
    void sendUpload(const ServerContract::Session& session,
                    const QString& documentId,
                    qint64 baseVersion,
                    const ServerContract::Upload& upload,
                    ServerContract::Reply<qint64> reply);
    void sendTakeLease(const ServerContract::Session& session,
                       const QString& documentId,
                       ServerContract::Reply<ServerContract::Lease> reply);
    void sendReleaseLease(const ServerContract::Session& session,
                          const QString& documentId,
                          ServerContract::Reply<ServerContract::Done> reply);

    QByteArray signature(ServerContract::Endpoint endpoint,
                         const ServerContract::Session& session,
                         const QString& documentId,
                         const QByteArray& body) const;

    ServerContract::ServerApi& m_server;
    DeviceIdentity m_identity;

    std::optional<ServerContract::Session> m_session;
    bool m_signingIn = false;
    QList<SessionReady> m_waiting;

    /// Guards every internal continuation, so a connection destroyed with a call
    /// outstanding is never called back.
    QObject m_lifetime;
};
