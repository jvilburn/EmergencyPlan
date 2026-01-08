#pragma once

#include <QWidget>
#include <QPoint>
#include <QPointF>
#include <QVariantMap>
#include <QSet>
#include <optional>

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
class MapHighlightProvider;

/// Pure C++ slippy map widget using TileService for tile rendering.
/// Replaces the QML-based implementation with QPainter rendering.
class MapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapWidget(DocumentManager* docManager, QWidget* parent = nullptr);
    ~MapWidget() override;

    /// Access the map viewmodel
    MapViewModel* viewModel() const { return m_viewModel; }

    /// Center the map on a specific family
    void centerOnFamily(const QString& familyId);

    /// Fit map to show all families
    void fitAllFamilies();

    /// Set which families are highlighted (from team/ministering selection)
    void setHighlightedFamilies(const QVariantMap& ids);

    /// Set which families are visible (from list filtering).
    /// Markers not in this set are de-emphasized. Empty set means all visible.
    void setVisibleFamilyIds(const QStringList& ids);

    /// Clear all highlighting
    void clearHighlighting();

    /// Set the map center location
    void setCenter(double lat, double lng);

    /// Set the highlight provider for marker coloring/opacity
    /// When set, the provider is queried instead of using m_highlightedIds/m_visibleIds
    void setHighlightProvider(MapHighlightProvider* provider);

signals:
    /// Emitted when a family marker is clicked
    void familyClicked(const QString& familyId);

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

private:
    void setupUi();
    void updateButtonPositions();
    void updateLayerButtonIcon();

    // Animation
    void animateTo(double lat, double lng, double zoom,
                   std::optional<LatLngBounds> targetBounds = std::nullopt,
                   QMarginsF contentPadding = QMarginsF());
    void stopAnimation();
    double easeOutCubic(double t);
    double easeInOutCubic(double t);
    LatLngBounds viewportBounds(double centerLat, double centerLng, double zoom) const;

    // Rendering
    void drawTiles(QPainter& painter);
    void drawMarkers(QPainter& painter);
    void drawControls(QPainter& painter);
    void drawAttribution(QPainter& painter);

    // Hit testing
    QString markerAtPoint(const QPoint& pos) const;

    // Bounds fitting
    void updateZoomForBounds();

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
    double m_animMidLat = 0.0;      // Center at mid-zoom (phase transition point)
    double m_animMidLng = 0.0;
    double m_animMidZoom = 0.0;     // Zoom level showing both start and target at opposite edges
    double m_animTargetLat = 0.0;
    double m_animTargetLng = 0.0;
    double m_animTargetZoom = 0.0;
    double m_animProgress = 0.0;    // 0.0 to 1.0
    bool m_animTwoPhase = false;    // True if zooming out then in
    LatLngBounds m_animStartBounds; // Viewport bounds at start
    LatLngBounds m_animTargetBounds; // Viewport bounds at target
    static constexpr int ANIM_DURATION_MS = 400;
    static constexpr int ANIM_TICK_MS = 16;  // ~60fps

    // Interaction state
    bool m_isDragging = false;
    QPoint m_lastMousePos;
    QPoint m_dragStartPos;
    bool m_wasDragging = false;  // To distinguish click from drag

    // Selection/highlighting
    QString m_selectedFamilyId;
    QVariantMap m_highlightedIds;  // familyId -> color string
    QSet<QString> m_visibleIds;    // Empty = all visible, otherwise only these are emphasized
    MapHighlightProvider* m_highlightProvider = nullptr;  // When set, overrides m_highlightedIds/m_visibleIds

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
