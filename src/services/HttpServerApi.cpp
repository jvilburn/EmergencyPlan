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
        reply(Result<T>::failure(HttpWire::readFailure(code, bytes)));
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
    if (call.signer.has_value())
    {
        for (const QPair<QByteArray, QByteArray>& header : HttpWire::signerHeaders(call.signer.value()))
        {
            request.setRawHeader(header.first, header.second);
        }
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

    // For a warning: the method and first segment only.
    const QString route = QString::fromLatin1(call.method) + " /" + call.path.section('/', 1, 1);
    connect(answer, &QNetworkReply::finished, this, std::bind(&answerFrom<T>, answer, read, reply, route));
}

void HttpServerApi::sendForDone(const Call& call, Reply<Done> reply)
{
    send<Done>(call, readDone, reply);
}

void HttpServerApi::registerDocument(const Signer& signer, const Upload& upload, Reply<qint64> reply)
{
    send<qint64>(Call{"POST", "/documents", HttpWire::registerRequest(upload), signer}, HttpWire::readVersion, reply);
}

void HttpServerApi::head(const QString& documentId, Reply<Head> reply)
{
    send<Head>(Call{"GET", documentPath(documentId) + "/head", std::nullopt, std::nullopt}, HttpWire::readHead, reply);
}

void HttpServerApi::download(const QString& documentId, Reply<Snapshot> reply)
{
    send<Snapshot>(Call{"GET", documentPath(documentId), std::nullopt, std::nullopt}, HttpWire::readSnapshot, reply);
}

void HttpServerApi::upload(const Signer& signer,
                           const QString& documentId,
                           qint64 baseVersion,
                           const Upload& upload,
                           Reply<qint64> reply)
{
    send<qint64>(Call{"PUT", documentPath(documentId), HttpWire::uploadRequest(baseVersion, upload), signer},
                 HttpWire::readVersion,
                 reply);
}

void HttpServerApi::takeLease(const Signer& signer, const QString& documentId, Reply<Lease> reply)
{
    send<Lease>(Call{"POST", documentPath(documentId) + "/lease", std::nullopt, signer}, HttpWire::readLease, reply);
}

void HttpServerApi::releaseLease(const Signer& signer, const QString& documentId, Reply<Done> reply)
{
    sendForDone(Call{"DELETE", documentPath(documentId) + "/lease", std::nullopt, signer}, reply);
}

void HttpServerApi::advanceClock(qint64 seconds, Reply<Done> reply)
{
    sendForDone(Call{"POST", "/dev/clock", HttpWire::advanceClockRequest(seconds), std::nullopt}, reply);
}
