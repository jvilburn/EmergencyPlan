#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "DocumentChange.h"
#include "Id.h"
#include "MarkerRenderer.h"

class DocumentManager;

/// ViewModel for the map widget. Manages family data caching and map view
/// state for MapWidget, reacting to document changes via DocumentManager.
class MapViewModel : public QObject
{
    Q_OBJECT

    // Family data for map markers
    Q_PROPERTY(QVariantList families READ families NOTIFY familiesChanged)

    // Map view state
    Q_PROPERTY(bool useSatelliteView READ useSatelliteView WRITE setUseSatelliteView NOTIFY satelliteViewChanged)
    Q_PROPERTY(double centerLat READ centerLat WRITE setCenterLat NOTIFY centerChanged)
    Q_PROPERTY(double centerLng READ centerLng WRITE setCenterLng NOTIFY centerChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)

public:
    explicit MapViewModel(DocumentManager* docManager, QObject* parent = nullptr);

    // Property getters
    QVariantList families() const;
    bool useSatelliteView() const { return m_useSatelliteView; }
    double centerLat() const { return m_centerLat; }
    double centerLng() const { return m_centerLng; }
    double zoom() const { return m_zoom; }

    // Property setters
    void setUseSatelliteView(bool satellite);
    void setCenterLat(double lat);
    void setCenterLng(double lng);
    void setZoom(double zoom);

    /// Get pre-computed marker icons for a family (computed when document changes)
    MarkerRenderer::MarkerIcons familyIcons(const FamilyId& familyId) const;

signals:
    void familiesChanged();
    void satelliteViewChanged();
    void centerChanged();
    void zoomChanged();

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void updateFamilies();
    QVariantMap familyToVariant(const class Family& family) const;

    DocumentManager* m_docManager;
    QVariantList m_families;
    QHash<FamilyId, MarkerRenderer::MarkerIcons> m_familyIcons;  // Pre-computed marker icons
    bool m_useSatelliteView = false;
    double m_centerLat = 40.0;
    double m_centerLng = -111.0;
    double m_zoom = 10.0;
};
