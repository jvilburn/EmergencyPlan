#pragma once

#include "GeocodingService.h"
#include "Family.h"

#include <QObject>
#include <QHash>

/// Background geocoding service with queue-based processing.
///
/// Features:
/// - Queue families for geocoding at any time
/// - Handles concurrent additions (add while running)
/// - Progress reporting via signals
/// - Can be stopped/cancelled
///
/// Usage:
///   service->queueFamily(family);
///
/// The caller decides what needs geocoding - this service
/// just processes whatever it's given.
class BackgroundGeocodingService : public QObject
{
    Q_OBJECT

public:
    explicit BackgroundGeocodingService(QObject* parent = nullptr);
    ~BackgroundGeocodingService() override;

    /// Queue a family for geocoding.
    void queueFamily(const Family& family);

    /// Stop all pending geocoding.
    void stop();

    /// Check if geocoding is in progress.
    bool isRunning() const;

    /// Get current progress (completed, total).
    int completed() const { return m_completed; }
    int total() const { return m_total; }

signals:
    /// Emitted when progress changes.
    void progressUpdated(int completed, int total);

    /// Emitted when a family is successfully geocoded.
    void familyGeocoded(const QString& id, double latitude, double longitude);

    /// Emitted when all queued families have been processed.
    void finished();

private slots:
    void onGeocodingComplete(const GeocodingResult& result);

private:
    GeocodingService* m_geocodingService;

    /// Map from address to family ID (for matching results)
    QHash<QString, QString> m_addressToFamilyId;

    /// Tracks families by ID to handle duplicates
    QSet<QString> m_queuedIds;

    int m_completed = 0;
    int m_total = 0;
};
