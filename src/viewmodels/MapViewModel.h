#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "DocumentChange.h"
#include "MarkerRenderer.h"

class DocumentManager;

/// ViewModel for the map, bridging DocumentManager and QML MapView.
/// Exposes family data and map state as Q_PROPERTY values for QML binding.
class MapViewModel : public QObject
{
    Q_OBJECT

    // Family data for map markers
    Q_PROPERTY(QVariantList families READ families NOTIFY familiesChanged)

    // Highlighting (from team/ministering selection)
    Q_PROPERTY(QVariantMap highlightedIds READ highlightedIds NOTIFY highlightingChanged)

    // Map view state
    Q_PROPERTY(bool useSatelliteView READ useSatelliteView WRITE setUseSatelliteView NOTIFY satelliteViewChanged)
    Q_PROPERTY(double centerLat READ centerLat WRITE setCenterLat NOTIFY centerChanged)
    Q_PROPERTY(double centerLng READ centerLng WRITE setCenterLng NOTIFY centerChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)

    // Selection
    Q_PROPERTY(QString selectedFamilyId READ selectedFamilyId WRITE setSelectedFamilyId NOTIFY selectionChanged)

public:
    explicit MapViewModel(DocumentManager* docManager, QObject* parent = nullptr);

    // Property getters
    QVariantList families() const;
    QVariantMap highlightedIds() const { return m_highlightedIds; }
    bool useSatelliteView() const { return m_useSatelliteView; }
    double centerLat() const { return m_centerLat; }
    double centerLng() const { return m_centerLng; }
    double zoom() const { return m_zoom; }
    QString selectedFamilyId() const { return m_selectedId; }

    // Property setters
    void setUseSatelliteView(bool satellite);
    void setCenterLat(double lat);
    void setCenterLng(double lng);
    void setZoom(double zoom);
    void setSelectedFamilyId(const QString& id);

    // QML-invokable methods
    Q_INVOKABLE void selectFamily(const QString& id);
    Q_INVOKABLE void centerOnFamily(const QString& id);
    Q_INVOKABLE void fitAllFamilies();
    Q_INVOKABLE void setHighlighting(const QVariantMap& ids);
    Q_INVOKABLE void clearHighlighting();

    // Called when map is clicked (for deselection)
    Q_INVOKABLE void mapClicked(double lat, double lng);

    /// Get pre-computed marker icons for a family (computed when document changes)
    MarkerRenderer::MarkerIcons familyIcons(const QString& familyId) const;

signals:
    void familiesChanged();
    void highlightingChanged();
    void satelliteViewChanged();
    void centerChanged();
    void zoomChanged();
    void selectionChanged();

    // Signal to C++ when a family marker is clicked
    void familyClicked(const QString& id);

    // Request to center map (handled by QML)
    void centerOnLocation(double lat, double lng, double zoomLevel);
    void fitBounds(double minLat, double minLng, double maxLat, double maxLng);

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void updateFamilies();
    QVariantMap familyToVariant(const class Family& family) const;

    DocumentManager* m_docManager;
    QVariantList m_families;
    QHash<QString, MarkerRenderer::MarkerIcons> m_familyIcons;  // Pre-computed marker icons
    QVariantMap m_highlightedIds;  // familyId -> color (as string)
    bool m_useSatelliteView = false;
    double m_centerLat = 40.0;
    double m_centerLng = -111.0;
    double m_zoom = 10.0;
    QString m_selectedId;
};
