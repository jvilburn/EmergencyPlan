#pragma once

#include <QObject>
#include <QDateTime>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

/// Result of a geocoding request.
struct GeocodingResult
{
    QString address;
    std::optional<double> latitude;
    std::optional<double> longitude;
    bool success = false;
};

/// Low-level geocoding service using Nominatim (OpenStreetMap).
///
/// Features:
/// - Rate limiting: 1.1 seconds between requests (Nominatim requires max 1/sec)
/// - Async operation with signal-based completion
/// - Fails silently on errors (returns empty coordinates)
///
/// This service handles single addresses. For batch processing,
/// use BackgroundGeocodingService.
class GeocodingService : public QObject
{
    Q_OBJECT

public:
    explicit GeocodingService(QObject* parent);
    ~GeocodingService() override;

    /// Queue an address for geocoding.
    /// Result will be emitted via geocodingComplete signal.
    void geocodeAddress(const QString& address);

    /// Check if there are pending requests.
    bool hasPendingRequests() const;

signals:
    /// Emitted when geocoding completes (success or failure).
    /// On failure, latitude/longitude will be nullopt.
    void geocodingComplete(const GeocodingResult& result);

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
    static constexpr const char* USER_AGENT = "WardPlanning/1.0";
};
