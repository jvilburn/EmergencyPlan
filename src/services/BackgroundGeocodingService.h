#pragma once

#include "GeocodingService.h"
#include "Family.h"
#include "Id.h"
#include "DocumentChange.h"

#include <QObject>
#include <QHash>
#include <QPointF>

/// Background geocoding service with reactive document change handling.
///
/// Features:
/// - Listens to documentChanged signal and automatically geocodes as needed
/// - Tracks per-family addresses to detect address changes
/// - Maintains address→coords cache for efficiency and undo/redo support
/// - Progress reporting via signals
/// - Can be stopped/cancelled
///
/// Behavior:
/// - Address changed → re-geocode (stale coords ignored)
/// - Address unchanged + mapped → trust existing coords (user corrections)
/// - Address unchanged + unmapped → use cache or queue API
class BackgroundGeocodingService : public QObject
{
    Q_OBJECT

public:
    explicit BackgroundGeocodingService(QObject* parent);
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
    void familyGeocoded(const FamilyId& id, double latitude, double longitude);

    /// Emitted when all queued families have been processed.
    void finished();

public slots:
    /// Handle document changes - seeds cache or queues families as needed.
    void onDocumentChanged(const DocumentChange& change);

private slots:
    void onGeocodingComplete(const GeocodingResult& result);

private:
    GeocodingService* m_geocodingService;

    /// Cache: address string → coordinates.
    /// Seeded from document on load, populated by API responses.
    QHash<QString, QPointF> m_geocodeCache;

    /// Tracks each family's last known address for change detection.
    QHash<FamilyId, QString> m_familyAddresses;

    /// Map from address to family ID (for matching API results)
    QHash<QString, FamilyId> m_addressToFamilyId;

    /// Tracks families by ID to handle duplicates in queue
    QSet<FamilyId> m_queuedIds;

    int m_completed = 0;
    int m_total = 0;

    /// Process all families on document load or batch change.
    void processAllFamilies();

    /// Check a single family against cache, queue or apply coords as needed.
    void checkFamily(const Family& family);
};
