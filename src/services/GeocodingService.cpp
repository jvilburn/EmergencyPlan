#include "GeocodingService.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

GeocodingService::GeocodingService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_rateLimitTimer(new QTimer(this))
{
    m_rateLimitTimer->setSingleShot(true);
    connect(m_rateLimitTimer, &QTimer::timeout,
            this, &GeocodingService::processNextRequest);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &GeocodingService::onRequestFinished);
}

GeocodingService::~GeocodingService() = default;

void GeocodingService::geocodeAddress(const QString& address)
{
    if (address.trimmed().isEmpty())
    {
        // Immediately emit failure for empty addresses
        emit geocodingComplete({address, std::nullopt, std::nullopt, false});
        return;
    }

    m_requestQueue.append(address);

    // Start processing if not already running
    if (!m_requestInFlight && !m_rateLimitTimer->isActive())
    {
        processNextRequest();
    }
}

bool GeocodingService::hasPendingRequests() const
{
    return !m_requestQueue.isEmpty() || m_requestInFlight;
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
    request.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = m_networkManager->get(request);
    reply->setProperty("address", address);
}

void GeocodingService::onRequestFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    m_requestInFlight = false;

    QString address = reply->property("address").toString();
    GeocodingResult result{address, std::nullopt, std::nullopt, false};

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);

        if (doc.isArray() && !doc.array().isEmpty())
        {
            QJsonObject obj = doc.array().first().toObject();
            bool latOk = false;
            bool lonOk = false;
            double lat = obj["lat"].toString().toDouble(&latOk);
            double lon = obj["lon"].toString().toDouble(&lonOk);

            if (latOk && lonOk)
            {
                result.latitude = lat;
                result.longitude = lon;
                result.success = true;
            }
        }
    }
    // Fail silently - no logging per user preference

    emit geocodingComplete(result);

    // Process next request after rate limit delay
    if (!m_requestQueue.isEmpty())
    {
        m_rateLimitTimer->start(RATE_LIMIT_MS);
    }
}
