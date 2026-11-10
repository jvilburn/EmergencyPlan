#pragma once

#include "Geocoder.h"

#include <QDateTime>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

/// The app's geocoder, over OpenStreetMap's Nominatim. Requests wait in a queue and go out
/// at most one every 1.1 seconds, as Nominatim requires. An address Nominatim does not find,
/// or a request that fails, answers with failed.
class GeocodingService : public Geocoder
{
    Q_OBJECT

public:
    explicit GeocodingService(QObject* parent);
    ~GeocodingService() override;

    void geocode(const QString& address) override;

private slots:
    void onRequestFinished(QNetworkReply* reply);
    void processNextRequest();

private:
    void startRequest(const QString& address);

    QNetworkAccessManager* m_networkManager;
    QTimer* m_rateLimitTimer;
    QStringList m_requestQueue;
    QDateTime m_lastRequestTime;
    bool m_requestInFlight = false;

    static constexpr int RATE_LIMIT_MS = 1100;  // 1.1 seconds
    static constexpr const char* NOMINATIM_URL = "https://nominatim.openstreetmap.org/search";
};
