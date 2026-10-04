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

} // namespace

ServerConnection::ServerConnection(ServerApi& server, const DeviceIdentity& identity)
    : m_server(server)
    , m_identity(identity)
{
}

// --- Endpoints -------------------------------------------------------------------

void ServerConnection::registerDocument(const Upload& upload, QObject* context, Reply<qint64> reply)
{
    call<qint64>(context, std::bind(&ServerConnection::sendRegisterDocument, this, _1, upload, _2), reply);
}

void ServerConnection::head(const QString& documentId, QObject* context, Reply<Head> reply)
{
    call<Head>(context, std::bind(&ServerApi::head, &m_server, _1, documentId, _2), reply);
}

void ServerConnection::download(const QString& documentId, qint64 after, QObject* context, Reply<Snapshot> reply)
{
    call<Snapshot>(context, std::bind(&ServerApi::download, &m_server, _1, documentId, after, _2), reply);
}

void ServerConnection::headers(const QString& documentId, qint64 after, QObject* context, Reply<HeaderChain> reply)
{
    call<HeaderChain>(context, std::bind(&ServerApi::headers, &m_server, _1, documentId, after, _2), reply);
}

void ServerConnection::upload(const QString& documentId,
                              qint64 baseVersion,
                              const Upload& upload,
                              QObject* context,
                              Reply<qint64> reply)
{
    call<qint64>(context,
                 std::bind(&ServerConnection::sendUpload, this, _1, documentId, baseVersion, upload, _2),
                 reply);
}

void ServerConnection::takeLease(const QString& documentId, QObject* context, Reply<Lease> reply)
{
    call<Lease>(context, std::bind(&ServerConnection::sendTakeLease, this, _1, documentId, _2), reply);
}

void ServerConnection::releaseLease(const QString& documentId, QObject* context, Reply<Done> reply)
{
    call<Done>(context, std::bind(&ServerConnection::sendReleaseLease, this, _1, documentId, _2), reply);
}

void ServerConnection::myDocuments(QObject* context, Reply<QStringList> reply)
{
    call<QStringList>(context, std::bind(&ServerApi::myDocuments, &m_server, _1, _2), reply);
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
        reply(Result<T>::failure(session.error));
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
        finishSignIn(Result<Session>::failure(challenge.error));
        return;
    }

    const QByteArray response =
        DocumentCrypto::sign(SignatureTag::Auth, challenge.value.value(), m_identity.signing.signingKey);
    m_server.authenticate(m_identity.signing.verifyKey,
                          challenge.value.value(),
                          response,
                          guarded<Session>(&m_lifetime, std::bind(&ServerConnection::finishSignIn, this, _1)));
}

void ServerConnection::finishSignIn(const Result<Session>& session)
{
    m_signingIn = false;
    if (session.ok())
    {
        m_session = session.value.value();
    }

    // Taken first: a waiting call that fails its sign-in may start another.
    const QList<SessionReady> waiting = m_waiting;
    m_waiting.clear();
    for (const SessionReady& ready : waiting)
    {
        ready(session);
    }
}

// --- Signed endpoints ------------------------------------------------------------

void ServerConnection::sendRegisterDocument(const Session& session, const Upload& upload, Reply<qint64> reply)
{
    m_server.registerDocument(session,
                              upload,
                              signature(Endpoint::RegisterDocument, session, upload.header.documentId, registerBody(upload)),
                              reply);
}

void ServerConnection::sendUpload(const Session& session,
                                  const QString& documentId,
                                  qint64 baseVersion,
                                  const Upload& upload,
                                  Reply<qint64> reply)
{
    m_server.upload(session,
                    documentId,
                    baseVersion,
                    upload,
                    signature(Endpoint::Upload, session, documentId, uploadBody(baseVersion, upload)),
                    reply);
}

void ServerConnection::sendTakeLease(const Session& session, const QString& documentId, Reply<Lease> reply)
{
    m_server.takeLease(session, documentId, signature(Endpoint::TakeLease, session, documentId, emptyBody()), reply);
}

void ServerConnection::sendReleaseLease(const Session& session, const QString& documentId, Reply<Done> reply)
{
    m_server.releaseLease(session,
                          documentId,
                          signature(Endpoint::ReleaseLease, session, documentId, emptyBody()),
                          reply);
}

QByteArray ServerConnection::signature(Endpoint endpoint,
                                       const Session& session,
                                       const QString& documentId,
                                       const QByteArray& body) const
{
    return signRequest(endpoint, session, documentId, body, m_identity.signing.signingKey);
}
