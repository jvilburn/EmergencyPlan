#include "GeocodingService.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

GeocodingService::GeocodingService(QObject* parent)
    : Geocoder(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_rateLimitTimer(new QTimer(this))
{
    // A request that receives nothing for Qt's default 30 seconds ends as an error, so a
    // stalled request cannot hold up the batch waiting on it.
    m_networkManager->setTransferTimeout();
    m_rateLimitTimer->setSingleShot(true);
    connect(m_rateLimitTimer, &QTimer::timeout,
            this, &GeocodingService::processNextRequest);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &GeocodingService::onRequestFinished);
}

GeocodingService::~GeocodingService() = default;

void GeocodingService::geocode(const QString& address)
{
    m_requestQueue.append(address);

    // Start processing if not already running
    if (!m_requestInFlight && !m_rateLimitTimer->isActive())
    {
        processNextRequest();
    }
}

void GeocodingService::processNextRequest()
{
    if (m_requestQueue.isEmpty() || m_requestInFlight)
    {
        return;
    }

    // Check rate limiting
    if (m_lastRequestTime.isValid())
    {
        qint64 elapsed = m_lastRequestTime.msecsTo(QDateTime::currentDateTime());
        if (elapsed < RATE_LIMIT_MS)
        {
            m_rateLimitTimer->start(RATE_LIMIT_MS - elapsed);
            return;
        }
    }

    QString address = m_requestQueue.takeFirst();
    startRequest(address);
}

void GeocodingService::startRequest(const QString& address)
{
    m_requestInFlight = true;
    m_lastRequestTime = QDateTime::currentDateTime();

    QUrl url(NOMINATIM_URL);
    QUrlQuery query;
    query.addQueryItem("q", address);
    query.addQueryItem("format", "json");
    query.addQueryItem("limit", "1");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "WardPlanning/" + QCoreApplication::applicationVersion());
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = m_networkManager->get(request);
    reply->setProperty("address", address);
}

void GeocodingService::onRequestFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    m_requestInFlight = false;

    QString address = reply->property("address").toString();
    std::optional<Coordinates> found;
    if (reply->error() == QNetworkReply::NoError)
    {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray() && !doc.array().isEmpty())
        {
            QJsonObject obj = doc.array().first().toObject();
            bool latOk = false;
            bool lonOk = false;
            double lat = obj["lat"].toString().toDouble(&latOk);
            double lon = obj["lon"].toString().toDouble(&lonOk);
            if (latOk && lonOk)
            {
                found = Coordinates(lat, lon);
            }
        }
    }

    if (found.has_value())
    {
        emit resolved(address, *found);
    }
    else
    {
        emit failed(address);
    }

    // Process next request after rate limit delay
    if (!m_requestQueue.isEmpty())
    {
        m_rateLimitTimer->start(RATE_LIMIT_MS);
    }
}
