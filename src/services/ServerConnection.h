#pragma once

#include "DeviceIdentity.h"
#include "ServerContract.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <optional>

/// One device's way to the server. It holds the session token and signs in on
/// first use - fetch a challenge, sign it, exchange it for a token, then register
/// this device's person when it has one. Everything above it calls the server
/// without seeing sessions.
///
/// An expired session is signed into again once: on Unauthenticated the connection
/// signs in afresh and repeats the call, and a second Unauthenticated is the
/// answer. Calls made while a sign-in is in flight wait for it, so a burst of calls
/// costs one sign-in. A device whose registration the server refuses as AccessLost
/// - one that was unlinked - hears that from every call, and the unlinked handler
/// hears it once.
///
/// Every reply is delivered only while its context is alive, as with
/// QTimer::singleShot: a session destroyed with a call outstanding never hears the
/// answer.
///
/// Tokens and request bodies stay in memory only; the bodies are ciphertext, and
/// keeping them out of logs keeps it that way.
class ServerConnection
{
public:
    /// identity may have no person yet: such a device can only sign in and link.
    ServerConnection(ServerContract::ServerApi& server, const DeviceIdentity& identity);

    ServerConnection(const ServerConnection&) = delete;
    ServerConnection& operator=(const ServerConnection&) = delete;

    /// Called once, from within the call that found out, when the server refuses
    /// this device's registration because it was unlinked.
    using Unlinked = std::function<void(const ServerContract::LostAccess& lost)>;
    void setUnlinkedHandler(Unlinked handler) { m_unlinked = std::move(handler); }

    // --- This device's person ---

    void me(QObject* context, ServerContract::Reply<ServerContract::Me> reply);
    void unlinkDevice(const Fingerprint& device, QObject* context, ServerContract::Reply<ServerContract::Done> reply);

    // --- Linking ---

    void startLink(QObject* context, ServerContract::Reply<QString> reply);
    void link(const QString& code, QObject* context, ServerContract::Reply<ServerContract::LinkState> reply);
    void approveLink(const QString& code, QObject* context, ServerContract::Reply<ServerContract::Done> reply);

    // --- Documents ---

    void registerDocument(const ServerContract::Upload& upload,
                          QObject* context,
                          ServerContract::Reply<qint64> reply);
    void head(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Head> reply);
    void download(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Snapshot> reply);
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

    // --- Who has access ---

    void history(const QString& documentId,
                 qint64 after,
                 QObject* context,
                 ServerContract::Reply<QList<ServerContract::HistoryEntry>> reply);
    void requestAccess(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Done> reply);
    void accessRequests(const QString& documentId,
                        QObject* context,
                        ServerContract::Reply<QList<ServerContract::AccessRequest>> reply);
    void declineRequest(const QString& documentId,
                        qint64 index,
                        QObject* context,
                        ServerContract::Reply<ServerContract::Done> reply);
    void myRequestStatus(const QString& documentId,
                         QObject* context,
                         ServerContract::Reply<ServerContract::RequestStatus> reply);
    void grant(const QString& documentId,
               qint64 index,
               QObject* context,
               ServerContract::Reply<ServerContract::Done> reply);
    void removeMember(const QString& documentId,
                      const MemberId& member,
                      QObject* context,
                      ServerContract::Reply<ServerContract::Done> reply);
    void setRole(const QString& documentId,
                 const MemberId& member,
                 ServerContract::Role role,
                 QObject* context,
                 ServerContract::Reply<ServerContract::Done> reply);

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
    void onAuthenticated(const ServerContract::Result<ServerContract::Session>& session);
    void onRegistered(const ServerContract::Session& session, const ServerContract::Result<ServerContract::Done>& registered);
    void finishSignIn(const ServerContract::Result<ServerContract::Session>& session);

    ServerContract::ServerApi& m_server;
    DeviceIdentity m_identity;

    std::optional<ServerContract::Session> m_session;

    /// Set once registration says this device was unlinked; every call answers it.
    std::optional<ServerContract::LostAccess> m_lost;
    Unlinked m_unlinked;
    bool m_signingIn = false;
    QList<SessionReady> m_waiting;

    /// Guards every internal continuation, so a connection destroyed with a call
    /// outstanding is never called back.
    QObject m_lifetime;
};
