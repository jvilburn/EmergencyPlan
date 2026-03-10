#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QVariantMap>
#include <QSet>
#include <optional>
#include "Id.h"

class QTimer;

struct LatLngBounds
{
    double minLat;
    double maxLat;
    double minLng;
    double maxLng;
};

class DocumentManager;
class MapViewModel;
class TileService;
class QPushButton;
class UnmappedPanel;
class FamilyMarkerProvider;

/// Pure C++ slippy map widget using TileService for tile rendering.
/// Replaces the QML-based implementation with QPainter rendering.
class MapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapWidget(DocumentManager* docManager, QWidget* parent);
    ~MapWidget() override;

    /// Access the map viewmodel
    MapViewModel* viewModel() const { return m_viewModel; }

    /// Fit map to show all families
    void fitAllFamilies();

    /// Set the map center location
    void setCenter(double lat, double lng);

    /// Set the marker provider for marker rendering
    void setMarkerProvider(FamilyMarkerProvider* provider);

public slots:
    /// Trigger repaint of both map and unmapped panel when highlights change
    void updateHighlights();

signals:
    /// Emitted after highlight processing completes
    void highlightChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onTileReady();
    void onZoomIn();
    void onZoomOut();
    void onToggleLayer();
    void onAnimationTick();
    void onGeocodingStarted();
    void onGeocodingFinished();
    void onUnmappedFamilyClicked(const FamilyId& familyId);
    void onUnmappedPanelHeaderClicked();

private:
    void setupUi();
    void updateButtonPositions();
    void updateLayerButtonIcon();

    // Animation
    void animateTo(double lat, double lng, double zoom);
    void stopAnimation();
    double easeOutCubic(double t);
    LatLngBounds viewportBounds(double centerLat, double centerLng, double zoom) const;

    // Rendering
    void drawTiles(QPainter& painter);
    void drawChurchMarkers(QPainter& painter);
    void drawMarkers(QPainter& painter);
    void drawControls(QPainter& painter);
    void drawAttribution(QPainter& painter);

    // Hit testing
    std::optional<FamilyId> markerAtPoint(const QPoint& pos) const;

    // Unmapped panel visibility
    void updateUnmappedPanelVisibility();
    bool hasUnmappedSelection() const;

    // Bounds fitting
    void updateZoomForBounds();
    void ensureVisible(const QSet<FamilyId>& familyIds);

    // Marker info for adaptive zoom calculation
    struct MarkerInfo
    {
        double lat;
        double lng;
        QMarginsF bounds;
    };

    // Result from adaptive zoom calculation
    struct AdaptiveZoomResult
    {
        double lat;
        double lng;
        double zoom;
    };

    // Calculate zoom and center that fits markers without UI collisions.
    AdaptiveZoomResult calculateAdaptiveZoom(const QVector<MarkerInfo>& markers) const;

    // Core state
    DocumentManager* m_docManager;
    MapViewModel* m_viewModel;

    // Map view state
    double m_centerLat = 40.0;
    double m_centerLng = -111.0;
    double m_zoom = 12.0;  // Double for smooth animation
    bool m_useSatellite = false;
    std::optional<LatLngBounds> m_bounds;  // If set, zoom adjusts on resize to fit

    // Animation state
    QTimer* m_animationTimer = nullptr;
    double m_animStartLat = 0.0;
    double m_animStartLng = 0.0;
    double m_animStartZoom = 0.0;
    double m_animTargetLat = 0.0;
    double m_animTargetLng = 0.0;
    double m_animTargetZoom = 0.0;
    double m_animProgress = 0.0;    // 0.0 to 1.0
    static constexpr int ANIM_DURATION_MS = 400;
    static constexpr int ANIM_TICK_MS = 16;  // ~60fps

    // Interaction state
    bool m_isDragging = false;
    QPoint m_lastMousePos;
    QPoint m_dragStartPos;
    bool m_wasDragging = false;  // To distinguish click from drag

    // Marker state
    FamilyMarkerProvider* m_markerProvider = nullptr;  // Provides marker state

    // Unmapped panel state
    bool m_isGeocoding = false;
    bool m_unmappedPanelManualOverride = false;

    // Attribution cache (pre-rendered pixmap, avoids drawText every paint)
    QRect m_attributionRect;
    QString m_cachedAttributionText;
    QPixmap m_cachedAttributionPixmap;

    // UI controls
    QPushButton* m_zoomInButton;
    QPushButton* m_zoomOutButton;
    QPushButton* m_recenterButton;
    QPushButton* m_layerButton;
    UnmappedPanel* m_unmappedPanel;

    // Constants
    static constexpr int MIN_ZOOM = 2;
    static constexpr int MAX_ZOOM = 19;
    static constexpr int DRAG_THRESHOLD = 5;  // Pixels to distinguish click from drag
};
