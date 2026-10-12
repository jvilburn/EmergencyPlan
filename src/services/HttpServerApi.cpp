#include "HttpServerApi.h"

#include "AppSettings.h"
#include "HttpWire.h"

#include <QDebug>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

using namespace ServerContract;
using namespace std::placeholders;

namespace
{

/// Answers reply from a finished request: read's reading of a 2xx body, or why
/// there is none. route names the call in a warning without anything it carried.
template<typename T>
void answerFrom(QNetworkReply* answer,
                const std::function<std::optional<T>(const QJsonObject&)>& read,
                const Reply<T>& reply,
                const QString& route)
{
    answer->deleteLater();

    // No HTTP answer at all - refused, timed out, no route, a TLS failure - or a
    // success cut off partway. A 4xx carries an error too, and its body says which.
    const QVariant status = answer->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const int code = status.isValid() ? status.toInt() : 0;
    const bool success = code >= 200 && code < 300;
    if (code == 0 || (success && answer->error() != QNetworkReply::NoError))
    {
        reply(Result<T>::failure(ServerError::Unreachable));
        return;
    }

    const QByteArray bytes = answer->readAll();
    if (!success)
    {
        const HttpWire::Failure failure = HttpWire::readFailure(code, bytes);
        reply(Result<T>{std::nullopt, failure.error, failure.lost});
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    const QJsonObject json = bytes.isEmpty() ? QJsonObject() : document.object();
    const std::optional<T> value = (bytes.isEmpty() || document.isObject()) ? read(json) : std::nullopt;
    if (!value.has_value())
    {
        // A server that answers in a shape this build cannot read is as good as
        // absent.
        qWarning() << "Unreadable server answer to" << route;
        reply(Result<T>::failure(ServerError::Unreachable));
        return;
    }
    reply(Result<T>::success(value.value()));
}

QString documentPath(const QString& documentId)
{
    return "/documents/" + QString::fromLatin1(QUrl::toPercentEncoding(documentId));
}

QString linkPath(const QString& code)
{
    return "/links/" + QString::fromLatin1(QUrl::toPercentEncoding(code));
}

QString memberPath(const QString& documentId, const MemberId& member)
{
    return documentPath(documentId) + "/members/" + member.toHex();
}

QString withQuery(const QString& path, const QString& key, const QString& value)
{
    return path + "?" + key + "=" + QString::fromLatin1(QUrl::toPercentEncoding(value));
}

std::optional<Done> readDone(const QJsonObject&)
{
    return Done{};
}

} // namespace

HttpServerApi::HttpServerApi(const AppSettings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
{
}

template<typename T>
void HttpServerApi::send(const Call& call,
                         std::function<std::optional<T>(const QJsonObject&)> read,
                         Reply<T> reply)
{
    QString base = m_settings.serverAddress();
    while (base.endsWith('/'))
    {
        base.chop(1);
    }
    const QUrl url(base + call.path);
    if (base.isEmpty() || !url.isValid())
    {
        // Still never before the call returns.
        QTimer::singleShot(0, this, [reply]() { reply(Result<T>::failure(ServerError::Unreachable)); });
        return;
    }

    QNetworkRequest request(url);
    request.setTransferTimeout(TransferTimeoutMilliseconds);
    if (!call.token.isEmpty())
    {
        request.setRawHeader("Authorization", "Bearer " + call.token.toUtf8());
    }

    QByteArray body;
    if (call.body.has_value())
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        body = QJsonDocument(call.body.value()).toJson(QJsonDocument::Compact);
    }

    QNetworkReply* answer = nullptr;
    if (call.method == "GET")
    {
        answer = m_network.get(request);
    }
    else if (call.method == "DELETE")
    {
        answer = m_network.deleteResource(request);
    }
    else
    {
        // A POST or PUT always says its length, an empty body included.
        request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
        answer = call.method == "PUT" ? m_network.put(request, body) : m_network.post(request, body);
    }

    // For a warning: the method and first segment only, since a path can carry a
    // link code.
    const QString route = QString::fromLatin1(call.method) + " /" + call.path.section('/', 1, 1);
    connect(answer, &QNetworkReply::finished, this, std::bind(&answerFrom<T>, answer, read, reply, route));
}

void HttpServerApi::sendForDone(const Call& call, Reply<Done> reply)
{
    send<Done>(call, readDone, reply);
}

// --- Signing in ------------------------------------------------------------------

void HttpServerApi::challenge(Reply<QByteArray> reply)
{
    send<QByteArray>(Call{"POST", "/auth/challenge", std::nullopt, QString()}, HttpWire::readChallenge, reply);
}

void HttpServerApi::authenticate(const VerifyKey& device,
                                 const QByteArray& challenge,
                                 const QByteArray& signature,
                                 Reply<Session> reply)
{
    send<Session>(Call{"POST", "/auth/token", HttpWire::authenticateRequest(device, challenge, signature), QString()},
                  HttpWire::readSession,
                  reply);
}

// --- This device's person --------------------------------------------------------

void HttpServerApi::registerPerson(const Session& session, const Registration& registration, Reply<Done> reply)
{
    sendForDone(Call{"PUT", "/me", HttpWire::registerPersonRequest(registration), session.token}, reply);
}

void HttpServerApi::me(const Session& session, Reply<Me> reply)
{
    send<Me>(Call{"GET", "/me", std::nullopt, session.token}, HttpWire::readMe, reply);
}

void HttpServerApi::unlinkDevice(const Session& session, const Fingerprint& device, Reply<Done> reply)
{
    sendForDone(Call{"DELETE", "/me/devices/" + device.toHex(), std::nullopt, session.token}, reply);
}

// --- Linking ---------------------------------------------------------------------

void HttpServerApi::startLink(const Session& session,
                              const PublicKey& publicKey,
                              const QString& deviceName,
                              Reply<QString> reply)
{
    send<QString>(Call{"POST", "/links", HttpWire::startLinkRequest(publicKey, deviceName), session.token},
                  HttpWire::readLinkCode,
                  reply);
}

void HttpServerApi::link(const Session& session, const QString& code, Reply<LinkState> reply)
{
    send<LinkState>(Call{"GET", linkPath(code), std::nullopt, session.token}, HttpWire::readLinkState, reply);
}

void HttpServerApi::approveLink(const Session& session,
                                const QString& code,
                                const QList<Resealing>& resealings,
                                Reply<Done> reply)
{
    sendForDone(Call{"POST", linkPath(code) + "/approve", HttpWire::approveLinkRequest(resealings), session.token},
                reply);
}

// --- Documents -------------------------------------------------------------------

void HttpServerApi::registerDocument(const Session& session, const Upload& upload, Reply<qint64> reply)
{
    send<qint64>(Call{"POST", "/documents", HttpWire::registerRequest(upload), session.token},
                 HttpWire::readVersion,
                 reply);
}

void HttpServerApi::head(const Session& session, const QString& documentId, Reply<Head> reply)
{
    send<Head>(Call{"GET", documentPath(documentId) + "/head", std::nullopt, session.token}, HttpWire::readHead, reply);
}

void HttpServerApi::download(const Session& session, const QString& documentId, Reply<Snapshot> reply)
{
    send<Snapshot>(Call{"GET", documentPath(documentId), std::nullopt, session.token}, HttpWire::readSnapshot, reply);
}

void HttpServerApi::upload(const Session& session,
                           const QString& documentId,
                           qint64 baseVersion,
                           const Upload& upload,
                           Reply<qint64> reply)
{
    send<qint64>(Call{"PUT", documentPath(documentId), HttpWire::uploadRequest(baseVersion, upload), session.token},
                 HttpWire::readVersion,
                 reply);
}

// --- Lease -----------------------------------------------------------------------

void HttpServerApi::takeLease(const Session& session, const QString& documentId, Reply<Lease> reply)
{
    send<Lease>(Call{"POST", documentPath(documentId) + "/lease", std::nullopt, session.token},
                HttpWire::readLease,
                reply);
}

void HttpServerApi::releaseLease(const Session& session, const QString& documentId, Reply<Done> reply)
{
    sendForDone(Call{"DELETE", documentPath(documentId) + "/lease", std::nullopt, session.token}, reply);
}

// --- Who has access --------------------------------------------------------------

void HttpServerApi::history(const Session& session,
                            const QString& documentId,
                            qint64 after,
                            Reply<QList<HistoryEntry>> reply)
{
    send<QList<HistoryEntry>>(
        Call{"GET", withQuery(documentPath(documentId) + "/history", "after", QString::number(after)), std::nullopt,
             session.token},
        HttpWire::readHistory,
        reply);
}

void HttpServerApi::requestAccess(const Session& session, const QString& documentId, Reply<Done> reply)
{
    sendForDone(Call{"POST", documentPath(documentId) + "/requests", std::nullopt, session.token}, reply);
}

void HttpServerApi::accessRequests(const Session& session,
                                   const QString& documentId,
                                   Reply<QList<AccessRequest>> reply)
{
    send<QList<AccessRequest>>(Call{"GET", documentPath(documentId) + "/requests", std::nullopt, session.token},
                               HttpWire::readAccessRequests,
                               reply);
}

void HttpServerApi::declineRequest(const Session& session, const QString& documentId, qint64 index, Reply<Done> reply)
{
    sendForDone(Call{"DELETE",
                     documentPath(documentId) + "/requests/" + QString::number(index),
                     std::nullopt,
                     session.token},
                reply);
}

void HttpServerApi::myRequestStatus(const Session& session, const QString& documentId, Reply<RequestStatus> reply)
{
    send<RequestStatus>(Call{"GET", documentPath(documentId) + "/requests/mine", std::nullopt, session.token},
                        HttpWire::readRequestStatus,
                        reply);
}

void HttpServerApi::grant(const Session& session,
                          const QString& documentId,
                          qint64 index,
                          const Resealing& resealing,
                          Reply<Done> reply)
{
    sendForDone(Call{"POST",
                     documentPath(documentId) + "/members",
                     HttpWire::grantRequest(index, resealing),
                     session.token},
                reply);
}

void HttpServerApi::removeMember(const Session& session,
                                 const QString& documentId,
                                 const MemberId& member,
                                 Reply<Done> reply)
{
    sendForDone(Call{"DELETE", memberPath(documentId, member), std::nullopt, session.token}, reply);
}

void HttpServerApi::setRole(const Session& session,
                            const QString& documentId,
                            const MemberId& member,
                            Role role,
                            Reply<Done> reply)
{
    sendForDone(Call{"PUT", memberPath(documentId, member) + "/role", HttpWire::setRoleRequest(role), session.token},
                reply);
}

// --- Development only ------------------------------------------------------------

void HttpServerApi::advanceClock(qint64 seconds, Reply<Done> reply)
{
    sendForDone(Call{"POST", "/dev/clock", HttpWire::advanceClockRequest(seconds), QString()}, reply);
}
