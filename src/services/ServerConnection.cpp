#include "ServerConnection.h"

using namespace ServerContract;
using namespace std::placeholders;

namespace
{

/// The reply, called only while context is alive.
template<typename T>
Reply<T> guarded(QObject* context, Reply<T> reply)
{
    const QPointer<QObject> alive(context);
    return [alive, reply](const Result<T>& result) { if (alive) { reply(result); } };
}

/// A failure carried from one result type to another, with its lost access.
template<typename T, typename U>
Result<T> failureOf(const Result<U>& result)
{
    return Result<T>{std::nullopt, result.error, result.lost};
}

} // namespace

ServerConnection::ServerConnection(ServerApi& server, const DeviceIdentity& identity)
    : m_server(server)
    , m_identity(identity)
{
}

// --- Endpoints -------------------------------------------------------------------

void ServerConnection::me(QObject* context, Reply<Me> reply)
{
    call<Me>(context, std::bind(&ServerApi::me, &m_server, _1, _2), reply);
}

void ServerConnection::unlinkDevice(const Fingerprint& device, QObject* context, Reply<Done> reply)
{
    call<Done>(context, std::bind(&ServerApi::unlinkDevice, &m_server, _1, device, _2), reply);
}

void ServerConnection::startLink(QObject* context, Reply<QString> reply)
{
    call<QString>(context,
                  std::bind(&ServerApi::startLink, &m_server, _1, m_identity.sealing.publicKey, m_identity.deviceName, _2),
                  reply);
}

void ServerConnection::link(const QString& code, QObject* context, Reply<LinkState> reply)
{
    call<LinkState>(context, std::bind(&ServerApi::link, &m_server, _1, code, _2), reply);
}

void ServerConnection::approveLink(const QString& code, QObject* context, Reply<Done> reply)
{
    call<Done>(context, std::bind(&ServerApi::approveLink, &m_server, _1, code, _2), reply);
}

void ServerConnection::registerDocument(const Upload& upload, QObject* context, Reply<qint64> reply)
{
    call<qint64>(context, std::bind(&ServerApi::registerDocument, &m_server, _1, upload, _2), reply);
}

void ServerConnection::head(const QString& documentId, QObject* context, Reply<Head> reply)
{
    call<Head>(context, std::bind(&ServerApi::head, &m_server, _1, documentId, _2), reply);
}

void ServerConnection::download(const QString& documentId, QObject* context, Reply<Snapshot> reply)
{
    call<Snapshot>(context, std::bind(&ServerApi::download, &m_server, _1, documentId, _2), reply);
}

void ServerConnection::upload(const QString& documentId,
                              qint64 baseVersion,
                              const Upload& upload,
                              QObject* context,
                              Reply<qint64> reply)
{
    call<qint64>(context, std::bind(&ServerApi::upload, &m_server, _1, documentId, baseVersion, upload, _2), reply);
}

void ServerConnection::takeLease(const QString& documentId, QObject* context, Reply<Lease> reply)
{
    call<Lease>(context, std::bind(&ServerApi::takeLease, &m_server, _1, documentId, _2), reply);
}

void ServerConnection::releaseLease(const QString& documentId, QObject* context, Reply<Done> reply)
{
    call<Done>(context, std::bind(&ServerApi::releaseLease, &m_server, _1, documentId, _2), reply);
}

void ServerConnection::history(const QString& documentId,
                               qint64 after,
                               QObject* context,
                               Reply<QList<HistoryEntry>> reply)
{
    call<QList<HistoryEntry>>(context, std::bind(&ServerApi::history, &m_server, _1, documentId, after, _2), reply);
}

void ServerConnection::requestAccess(const QString& documentId, QObject* context, Reply<Done> reply)
{
    call<Done>(context, std::bind(&ServerApi::requestAccess, &m_server, _1, documentId, _2), reply);
}

void ServerConnection::accessRequests(const QString& documentId, QObject* context, Reply<QList<AccessRequest>> reply)
{
    call<QList<AccessRequest>>(context, std::bind(&ServerApi::accessRequests, &m_server, _1, documentId, _2), reply);
}

void ServerConnection::declineRequest(const QString& documentId, qint64 index, QObject* context, Reply<Done> reply)
{
    call<Done>(context, std::bind(&ServerApi::declineRequest, &m_server, _1, documentId, index, _2), reply);
}

void ServerConnection::myRequestStatus(const QString& documentId, QObject* context, Reply<RequestStatus> reply)
{
    call<RequestStatus>(context, std::bind(&ServerApi::myRequestStatus, &m_server, _1, documentId, _2), reply);
}

void ServerConnection::grant(const QString& documentId, qint64 index, QObject* context, Reply<Done> reply)
{
    call<Done>(context, std::bind(&ServerApi::grant, &m_server, _1, documentId, index, _2), reply);
}

void ServerConnection::removeMember(const QString& documentId,
                                    const MemberId& member,
                                    QObject* context,
                                    Reply<Done> reply)
{
    call<Done>(context, std::bind(&ServerApi::removeMember, &m_server, _1, documentId, member, _2), reply);
}

void ServerConnection::setRole(const QString& documentId,
                               const MemberId& member,
                               Role role,
                               QObject* context,
                               Reply<Done> reply)
{
    call<Done>(context, std::bind(&ServerApi::setRole, &m_server, _1, documentId, member, role, _2), reply);
}

// --- One call, signed in once more if its session has expired ----------------------

template<typename T>
void ServerConnection::call(QObject* context, Attempt<T> attempt, Reply<T> reply)
{
    withSession(std::bind(&ServerConnection::attemptWith<T>, this, _1, attempt, guarded(context, reply), true));
}

template<typename T>
void ServerConnection::attemptWith(const Result<Session>& session,
                                   Attempt<T> attempt,
                                   Reply<T> reply,
                                   bool mayRetry)
{
    if (!session.ok())
    {
        reply(failureOf<T>(session));
        return;
    }

    const QString token = session.value.value().token;
    attempt(session.value.value(),
            guarded<T>(&m_lifetime,
                       std::bind(&ServerConnection::answered<T>, this, token, _1, attempt, reply, mayRetry)));
}

template<typename T>
void ServerConnection::answered(const QString& token,
                                const Result<T>& result,
                                Attempt<T> attempt,
                                Reply<T> reply,
                                bool mayRetry)
{
    if (result.error != ServerError::Unauthenticated || !mayRetry)
    {
        reply(result);
        return;
    }

    // Forgotten only when it is still the session in use: a burst of calls that all
    // failed on one expired session signs in once, not once each.
    if (m_session.has_value() && m_session.value().token == token)
    {
        m_session.reset();
    }

    withSession(std::bind(&ServerConnection::attemptWith<T>, this, _1, attempt, reply, false));
}

// --- Signing in ------------------------------------------------------------------

void ServerConnection::withSession(SessionReady ready)
{
    if (m_session.has_value())
    {
        ready(Result<Session>::success(m_session.value()));
        return;
    }
    if (m_lost.has_value())
    {
        ready(Result<Session>::lostAccess(m_lost.value()));
        return;
    }

    m_waiting.append(ready);
    if (m_signingIn)
    {
        return;
    }

    m_signingIn = true;
    m_server.challenge(guarded<QByteArray>(&m_lifetime, std::bind(&ServerConnection::onChallenge, this, _1)));
}

void ServerConnection::onChallenge(const Result<QByteArray>& challenge)
{
    if (!challenge.ok())
    {
        finishSignIn(failureOf<Session>(challenge));
        return;
    }

    const QByteArray response = DocumentCrypto::signChallenge(challenge.value.value(), m_identity.signing.signingKey);
    m_server.authenticate(m_identity.signing.verifyKey,
                          challenge.value.value(),
                          response,
                          guarded<Session>(&m_lifetime, std::bind(&ServerConnection::onAuthenticated, this, _1)));
}

void ServerConnection::onAuthenticated(const Result<Session>& session)
{
    // A device with no person yet signs in to link, and has nothing to register.
    if (!session.ok() || !m_identity.member.isValid())
    {
        finishSignIn(session);
        return;
    }

    m_server.registerPerson(session.value.value(),
                            m_identity.registration(),
                            guarded<Done>(&m_lifetime,
                                          std::bind(&ServerConnection::onRegistered, this, session.value.value(), _1)));
}

void ServerConnection::onRegistered(const Session& session, const Result<Done>& registered)
{
    finishSignIn(registered.ok() ? Result<Session>::success(session) : failureOf<Session>(registered));
}

void ServerConnection::finishSignIn(const Result<Session>& session)
{
    m_signingIn = false;
    if (session.ok())
    {
        m_session = session.value.value();
    }
    if (session.error == ServerError::AccessLost)
    {
        m_lost = session.lost;
    }

    // Taken first: a waiting call that fails its sign-in may start another.
    const QList<SessionReady> waiting = m_waiting;
    m_waiting.clear();
    for (const SessionReady& ready : waiting)
    {
        ready(session);
    }
}
