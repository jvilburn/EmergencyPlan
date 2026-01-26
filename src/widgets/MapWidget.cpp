#include "MapWidget.h"
#include "FamilyMarkerProvider.h"
#include "SlippyMapMath.h"
#include "MarkerRenderer.h"
#include "UnmappedPanel.h"
#include "MapViewModel.h"
#include "DocumentManager.h"
#include "DocumentChange.h"
#include "TileService.h"
#include "Family.h"
#include "Ward.h"
#include "Stake.h"
#include "Document.h"

#include <QPainter>
#include <QPushButton>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QtMath>
#include <QDebug>

MapWidget::MapWidget(DocumentManager* docManager, QWidget* parent)
    : QWidget(parent)
    , m_docManager(docManager)
    , m_viewModel(new MapViewModel(docManager, this))
{
    setObjectName("mapWidget");
    setMinimumWidth(400);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Animation timer
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(ANIM_TICK_MS);
    connect(m_animationTimer, &QTimer::timeout, this, &MapWidget::onAnimationTick);

    setupUi();

    // Connect to tile service for repaints when tiles load
    TileService* tileService = TileService::instance();
    if (tileService)
    {
        connect(tileService, &TileService::tileReady,
                this, &MapWidget::onTileReady);
    }

    // Connect viewmodel signals
    connect(m_viewModel, &MapViewModel::familiesChanged,
            this, QOverload<>::of(&QWidget::update));
    connect(m_viewModel, &MapViewModel::highlightingChanged,
            this, QOverload<>::of(&QWidget::update));
    connect(m_viewModel, &MapViewModel::familyClicked,
            this, &MapWidget::familyClicked);

    // Update button positions when document changes (affects unmapped panel)
    connect(m_docManager, &DocumentManager::documentChanged,
            this, [this](const DocumentChange&) { updateButtonPositions(); });
}

MapWidget::~MapWidget()
{
}

void MapWidget::setupUi()
{
    // Zoom in button
    m_zoomInButton = new QPushButton(this);
    m_zoomInButton->setObjectName("mapZoomButton");
    m_zoomInButton->setFixedSize(32, 32);
    m_zoomInButton->setIcon(QIcon(":/icons/zoom-in.svg"));
    m_zoomInButton->setIconSize(QSize(20, 20));
    m_zoomInButton->setCursor(Qt::PointingHandCursor);
    QGraphicsDropShadowEffect* zoomInShadow = new QGraphicsDropShadowEffect(m_zoomInButton);
    zoomInShadow->setBlurRadius(6);
    zoomInShadow->setOffset(0, 1);
    zoomInShadow->setColor(QColor(0, 0, 0, 60));
    m_zoomInButton->setGraphicsEffect(zoomInShadow);
    connect(m_zoomInButton, &QPushButton::clicked, this, &MapWidget::onZoomIn);

    // Zoom out button
    m_zoomOutButton = new QPushButton(this);
    m_zoomOutButton->setObjectName("mapZoomButton");
    m_zoomOutButton->setFixedSize(32, 32);
    m_zoomOutButton->setIcon(QIcon(":/icons/zoom-out.svg"));
    m_zoomOutButton->setIconSize(QSize(20, 20));
    m_zoomOutButton->setCursor(Qt::PointingHandCursor);
    QGraphicsDropShadowEffect* zoomOutShadow = new QGraphicsDropShadowEffect(m_zoomOutButton);
    zoomOutShadow->setBlurRadius(6);
    zoomOutShadow->setOffset(0, 1);
    zoomOutShadow->setColor(QColor(0, 0, 0, 60));
    m_zoomOutButton->setGraphicsEffect(zoomOutShadow);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &MapWidget::onZoomOut);

    // Recenter button - fit all families in view
    m_recenterButton = new QPushButton(this);
    m_recenterButton->setFixedSize(32, 32);
    m_recenterButton->setIcon(QIcon(":/icons/recenter.svg"));
    m_recenterButton->setIconSize(QSize(20, 20));
    m_recenterButton->setCursor(Qt::PointingHandCursor);
    m_recenterButton->setToolTip(tr("Fit all families"));
    QGraphicsDropShadowEffect* recenterShadow = new QGraphicsDropShadowEffect(m_recenterButton);
    recenterShadow->setBlurRadius(6);
    recenterShadow->setOffset(0, 1);
    recenterShadow->setColor(QColor(0, 0, 0, 60));
    m_recenterButton->setGraphicsEffect(recenterShadow);
    connect(m_recenterButton, &QPushButton::clicked, this, &MapWidget::fitAllFamilies);

    // Layer toggle button - shows icon of the OTHER layer (what you'll switch to)
    m_layerButton = new QPushButton(this);
    m_layerButton->setFixedSize(66, 66);
    m_layerButton->setCursor(Qt::PointingHandCursor);
    // Add shadow for visibility against the map
    QGraphicsDropShadowEffect* layerShadow = new QGraphicsDropShadowEffect(m_layerButton);
    layerShadow->setBlurRadius(8);
    layerShadow->setOffset(0, 2);
    layerShadow->setColor(QColor(0, 0, 0, 80));
    m_layerButton->setGraphicsEffect(layerShadow);
    updateLayerButtonIcon();
    connect(m_layerButton, &QPushButton::clicked, this, &MapWidget::onToggleLayer);

    // Unmapped families panel
    m_unmappedPanel = new UnmappedPanel(m_docManager, this);
    connect(m_unmappedPanel, &UnmappedPanel::familyClicked,
            this, &MapWidget::familyClicked);
    connect(this, &MapWidget::highlightChanged,
            m_unmappedPanel, QOverload<>::of(&QWidget::update));
    connect(m_unmappedPanel, &UnmappedPanel::headerClicked,
            this, &MapWidget::onUnmappedPanelHeaderClicked);

    // Geocoding state for unmapped panel visibility
    connect(m_docManager, &DocumentManager::geocodingProgressChanged,
            this, &MapWidget::onGeocodingStarted);
    connect(m_docManager, &DocumentManager::geocodingFinished,
            this, &MapWidget::onGeocodingFinished);

    updateButtonPositions();
}

void MapWidget::updateButtonPositions()
{
    int margin = 10;

    // Zoom and recenter buttons in top-left
    m_zoomInButton->move(margin, margin);
    m_zoomOutButton->move(margin, margin + 36);
    m_recenterButton->move(margin, margin + 72);

    // Layer button in top-right
    m_layerButton->move(width() - m_layerButton->width() - margin, margin);

    // Unmapped panel at bottom-right, growing upward
    if (m_unmappedPanel->hasUnmappedFamilies())
    {
        m_unmappedPanel->setVisible(true);
        QSize panelSize = m_unmappedPanel->sizeHint();
        // Clamp height to widget height
        int panelHeight = qMin(panelSize.height(), height());
        m_unmappedPanel->setFixedSize(panelSize.width(), panelHeight);
        m_unmappedPanel->move(width() - panelSize.width(), height() - panelHeight);
    }
    else
    {
        m_unmappedPanel->setVisible(false);
    }
}

void MapWidget::updateUnmappedPanelVisibility()
{
    if (!m_unmappedPanel->hasUnmappedFamilies())
    {
        return;  // Nothing to show/hide
    }

    bool shouldExpand = m_isGeocoding || hasUnmappedSelection();

    if (shouldExpand)
    {
        // Auto-expand clears manual override
        m_unmappedPanelManualOverride = false;
        m_unmappedPanel->setExpanded(true);
    }
    else if (!m_unmappedPanelManualOverride)
    {
        // Auto-collapse only if no manual override
        m_unmappedPanel->setExpanded(false);
    }

    updateButtonPositions();
}

bool MapWidget::hasUnmappedSelection() const
{
    if (!m_markerProvider)
    {
        return false;
    }

    QSet<QString> highlighted = m_markerProvider->highlightInfo().allHighlightedIds();
    const Document& doc = m_docManager->document();

    for (const QString& id : highlighted)
    {
        std::optional<Family> family = doc.findFamilyById(id);
        if (family && !family->isMapped())
        {
            return true;
        }
    }

    return false;
}

void MapWidget::onGeocodingStarted()
{
    m_isGeocoding = true;
    updateUnmappedPanelVisibility();
}

void MapWidget::onGeocodingFinished()
{
    m_isGeocoding = false;
    updateUnmappedPanelVisibility();
}

void MapWidget::onUnmappedPanelHeaderClicked()
{
    m_unmappedPanelManualOverride = true;
    m_unmappedPanel->setExpanded(!m_unmappedPanel->isExpanded());
    updateButtonPositions();
}

void MapWidget::updateLayerButtonIcon()
{
    QString tooltip = m_useSatellite ? "Switch to Street" : "Switch to Satellite";
    m_layerButton->setToolTip(tooltip);

    TileService* tileService = TileService::instance();
    if (!tileService || !tileService->isInitialized())
    {
        return;
    }

    // Match exactly what drawTiles() does for coordinate calculation
    double clampedZoom = qBound(static_cast<double>(MIN_ZOOM), m_zoom, static_cast<double>(MAX_ZOOM));
    int tileZoom = static_cast<int>(qFloor(clampedZoom));
    double zoomFraction = clampedZoom - tileZoom;
    double scale = qPow(2.0, zoomFraction);
    double scaledTileSize = 256.0 * scale;

    double n = 1 << tileZoom;
    double centerTileX = ((m_centerLng + 180.0) / 360.0) * n;
    double centerTileY = SlippyMapMath::latToTileY(m_centerLat, tileZoom);

    double offsetX = (centerTileX - qFloor(centerTileX)) * scaledTileSize;
    double offsetY = (centerTileY - qFloor(centerTileY)) * scaledTileSize;

    // Button center in screen coordinates
    QPoint buttonCenter = m_layerButton->pos() + QPoint(m_layerButton->width() / 2,
                                                         m_layerButton->height() / 2);

    // Reverse the screen position formula from drawTiles to find tile coordinates
    // screenX = width/2 - offsetX + (tx - floor(centerTileX)) * scaledTileSize
    double tileXFloat = qFloor(centerTileX)
                        + (buttonCenter.x() - width() / 2.0 + offsetX) / scaledTileSize;
    double tileYFloat = qFloor(centerTileY)
                        + (buttonCenter.y() - height() / 2.0 + offsetY) / scaledTileSize;

    int tileX = static_cast<int>(qFloor(tileXFloat));
    int tileY = static_cast<int>(qFloor(tileYFloat));

    // Clamp to valid range
    tileX = qBound(0, tileX, static_cast<int>(n) - 1);
    tileY = qBound(0, tileY, static_cast<int>(n) - 1);

    // Get tile for the OTHER layer
    TileLayer otherLayer = m_useSatellite ? TileLayer::Street : TileLayer::Satellite;
    QPixmap tile = tileService->getTile(TileId(otherLayer, tileZoom, tileX, tileY));

    if (tile.isNull())
    {
        return;
    }

    // Position within tile (0.0 to 1.0)
    double fracX = tileXFloat - tileX;
    double fracY = tileYFloat - tileY;

    // Position within tile's 256px image
    double pixelX = fracX * 256.0;
    double pixelY = fracY * 256.0;

    // To match the scaled display, crop (iconSize/scale) pixels and scale up
    constexpr int buttonIconSize = 54;
    double cropSize = buttonIconSize / scale;

    double cropX = pixelX - cropSize / 2.0;
    double cropY = pixelY - cropSize / 2.0;

    // Clamp to tile bounds
    cropX = qBound(0.0, cropX, 256.0 - cropSize);
    cropY = qBound(0.0, cropY, 256.0 - cropSize);

    // Crop and scale to match the map's display
    int cropSizeInt = qMax(1, static_cast<int>(qCeil(cropSize)));
    QPixmap cropped = tile.copy(static_cast<int>(cropX), static_cast<int>(cropY),
                                 cropSizeInt, cropSizeInt);
    QPixmap scaled = cropped.scaled(buttonIconSize, buttonIconSize,
                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    m_layerButton->setIcon(QIcon(scaled));
    m_layerButton->setIconSize(QSize(buttonIconSize, buttonIconSize));
}

void MapWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateButtonPositions();
    updateZoomForBounds();
}

void MapWidget::updateZoomForBounds()
{
    if (!m_bounds)
    {
        return;
    }

    if (width() <= 0 || height() <= 0)
    {
        return;
    }

    double latRange = m_bounds->maxLat - m_bounds->minLat;
    double lngRange = m_bounds->maxLng - m_bounds->minLng;
    double centerLat = (m_bounds->minLat + m_bounds->maxLat) / 2.0;

    // Find zoom level that fits both dimensions
    for (int z = MAX_ZOOM; z >= MIN_ZOOM; --z)
    {
        double degreesPerPixelLng = SlippyMapMath::lngDegreesPerPixel(z);
        double degreesPerPixelLat = SlippyMapMath::latDegreesPerPixel(z, centerLat);

        double widgetLngRange = width() * degreesPerPixelLng;
        double widgetLatRange = height() * degreesPerPixelLat;

        if (widgetLngRange >= lngRange && widgetLatRange >= latRange)
        {
            m_zoom = static_cast<double>(z);
            break;
        }
    }
}

// ============================================================================
// Painting
// ============================================================================

void MapWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw tiles first (background)
    drawTiles(painter);

    // Draw family markers
    drawMarkers(painter);

    // Draw church markers on top (landmark reference points)
    drawChurchMarkers(painter);

    // Draw attribution
    drawAttribution(painter);
}

void MapWidget::drawTiles(QPainter& painter)
{
    TileService* tileService = TileService::instance();
    if (!tileService || !tileService->isInitialized())
    {
        // Draw placeholder background
        painter.fillRect(rect(), QColor(200, 200, 200));
        painter.drawText(rect(), Qt::AlignCenter, "Loading tiles...");
        return;
    }

    TileLayer layer = m_useSatellite ? TileLayer::Satellite : TileLayer::Street;

    // Use floor of zoom for tile fetching (not round) - we'll scale tiles for fractional zoom
    double clampedZoom = qBound(static_cast<double>(MIN_ZOOM), m_zoom, static_cast<double>(MAX_ZOOM));
    int tileZoom = static_cast<int>(qFloor(clampedZoom));

    // Scale factor for fractional zoom: when zoom is 12.5, tiles at z12 are drawn at 141% size
    double zoomFraction = clampedZoom - tileZoom;
    double scale = qPow(2.0, zoomFraction);
    double scaledTileSize = SlippyMapMath::TILE_SIZE * scale;

    double n = 1 << tileZoom;

    // Calculate center tile position at the tile zoom level
    double centerTileX = ((m_centerLng + 180.0) / 360.0) * n;
    double centerTileY = SlippyMapMath::latToTileY(m_centerLat, tileZoom);

    // Calculate how many tiles we need to cover the widget (use scaled tile size)
    int tilesX = static_cast<int>(qCeil(width() / scaledTileSize)) + 3;
    int tilesY = static_cast<int>(qCeil(height() / scaledTileSize)) + 3;

    // Calculate the range of tiles to draw
    int startTileX = static_cast<int>(qFloor(centerTileX)) - tilesX / 2;
    int startTileY = static_cast<int>(qFloor(centerTileY)) - tilesY / 2;
    int endTileX = startTileX + tilesX;
    int endTileY = startTileY + tilesY;

    // Pixel offset for the first tile (scaled)
    double offsetX = (centerTileX - qFloor(centerTileX)) * scaledTileSize;
    double offsetY = (centerTileY - qFloor(centerTileY)) * scaledTileSize;

    int maxTile = (1 << tileZoom) - 1;

    // Enable smooth scaling for better visual quality during zoom animation
    painter.setRenderHint(QPainter::SmoothPixmapTransform, scale != 1.0);

    for (int ty = startTileY; ty <= endTileY; ++ty)
    {
        for (int tx = startTileX; tx <= endTileX; ++tx)
        {
            // Wrap X coordinate (longitude wraps around)
            int wrappedX = tx;
            while (wrappedX < 0)
            {
                wrappedX += (1 << tileZoom);
            }
            wrappedX = wrappedX % (1 << tileZoom);

            // Skip tiles outside Y bounds (latitude doesn't wrap)
            if (ty < 0 || ty > maxTile)
            {
                continue;
            }

            // Calculate screen position for this tile (using scaled tile size)
            double screenX = width() / 2.0 - offsetX
                             + (tx - qFloor(centerTileX)) * scaledTileSize;
            double screenY = height() / 2.0 - offsetY
                             + (ty - qFloor(centerTileY)) * scaledTileSize;

            // Get tile from service (always returns something drawable)
            QPixmap tile = tileService->getTile(TileId(layer, tileZoom, wrappedX, ty));

            // Draw the tile scaled
            QRectF targetRect(screenX, screenY, scaledTileSize, scaledTileSize);
            painter.drawPixmap(targetRect, tile, tile.rect());
        }
    }

    // Reset smooth transform hint
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
}

void MapWidget::drawChurchMarkers(QPainter& painter)
{
    const Document& doc = m_docManager->document();
    const QHash<QString, Ward>& wards = doc.wards();
    const QHash<QString, Stake>& stakes = doc.stakes();

    if (wards.isEmpty() && stakes.isEmpty())
    {
        return;
    }

    // Collect unique building locations (deduplicate shared buildings)
    // Key: rounded lat/lng to 4 decimal places (~11m precision)
    QSet<QPair<qint64, qint64>> seenLocations;
    QVector<QPair<double, double>> buildingLocations;

    auto addLocation = [&](double lat, double lng)
    {
        // Round to 4 decimal places for deduplication
        qint64 latKey = static_cast<qint64>(lat * 10000);
        qint64 lngKey = static_cast<qint64>(lng * 10000);
        QPair<qint64, qint64> key(latKey, lngKey);

        if (!seenLocations.contains(key))
        {
            seenLocations.insert(key);
            buildingLocations.append({lat, lng});
        }
    };

    // Collect ward chapel locations
    for (const Ward& ward : wards)
    {
        if (ward.chapelLat() && ward.chapelLng())
        {
            addLocation(ward.chapelLat().value(), ward.chapelLng().value());
        }
    }

    // Collect stake center locations
    for (const Stake& stake : stakes)
    {
        if (stake.stakeCenterLat() && stake.stakeCenterLng())
        {
            addLocation(stake.stakeCenterLat().value(), stake.stakeCenterLng().value());
        }
    }

    // Draw markers for unique locations
    double clampedZoom = qBound(static_cast<double>(MIN_ZOOM), m_zoom, static_cast<double>(MAX_ZOOM));
    double markerExtent = MarkerRenderer::CHURCH_MARKER_SIZE / 2.0;

    for (const auto& [lat, lng] : buildingLocations)
    {
        QPointF pos = SlippyMapMath::latLngToPixel(lat, lng, clampedZoom,
                                                    m_centerLat, m_centerLng,
                                                    width(), height());

        // Skip if outside visible area
        if (pos.x() < -markerExtent || pos.x() > width() + markerExtent
            || pos.y() < -markerExtent || pos.y() > height() + markerExtent)
        {
            continue;
        }

        MarkerRenderer::State state;
        state.opacity = 1.0;

        MarkerRenderer::drawChurch(painter, pos, state);
    }
}

void MapWidget::drawMarkers(QPainter& painter)
{
    const QVariantList& families = m_viewModel->families();

    // Use fractional zoom for smooth marker positioning during animation
    double clampedZoom = qBound(static_cast<double>(MIN_ZOOM), m_zoom, static_cast<double>(MAX_ZOOM));

    // If we have a highlight provider, use it for all highlighting decisions
    if (m_markerProvider)
    {
        QSet<QString> visibleIds = m_markerProvider->visibleFamilyIds();
        bool hasVisibleFilter = !visibleIds.isEmpty();

        // Get highlight info once (semantic data from provider)
        HighlightInfo info = m_markerProvider->highlightInfo();
        QSet<QString> allHighlighted = info.allHighlightedIds();
        bool hasHighlighting = info.hasHighlighting();

        // Collect markers with their draw order (low opacity first, high opacity on top)
        struct MarkerInfo
        {
            QPointF pos;
            QVariantMap data;
            MarkerRenderer::State state;
            qreal sortOpacity;
        };
        QVector<MarkerInfo> markers;

        for (const QVariant& var : families)
        {
            QVariantMap hh = var.toMap();
            QString id = hh["id"].toString();

            // Skip if not in visible set (when filtering is active)
            if (hasVisibleFilter && !visibleIds.contains(id))
            {
                continue;
            }

            double lat = hh["latitude"].toDouble();
            double lng = hh["longitude"].toDouble();
            QPointF pos = SlippyMapMath::latLngToPixel(lat, lng, clampedZoom,
                                                        m_centerLat, m_centerLng,
                                                        width(), height());

            if (!MarkerRenderer::isVisible(pos, width(), height()))
            {
                continue;
            }

            bool isHighlighted = allHighlighted.contains(id);
            bool isContactPoint = info.contactPointFamilyIds.contains(id);
            QString statusIcon = m_markerProvider->familyStatusIcon(id);

            // Opacity: dim non-highlighted when highlighting is active
            qreal opacity = (hasHighlighting && !isHighlighted) ? 0.3 : 1.0;

            MarkerRenderer::State state;
            state.isHighlighted = isHighlighted;
            state.showPip = isContactPoint;
            state.opacity = opacity;
            state.statusIcon = statusIcon;
            state.icons = MarkerRenderer::computeFamilyIcons(id, m_docManager->document());

            markers.append({pos, hh, state, opacity});
        }

        // Sort by opacity, then by pip (so pip markers draw on top)
        std::sort(markers.begin(), markers.end(),
                  [](const MarkerInfo& a, const MarkerInfo& b)
                  {
                      if (a.sortOpacity != b.sortOpacity)
                      {
                          return a.sortOpacity < b.sortOpacity;
                      }
                      // Pip markers draw last (on top)
                      return !a.state.showPip && b.state.showPip;
                  });

        for (const MarkerInfo& m : markers)
        {
            MarkerRenderer::draw(painter, m.pos, m.data, m.state);
        }
        return;
    }

    Q_ASSERT_X(false, "MapWidget::drawMarkers", "No highlight provider set");
}

void MapWidget::drawControls(QPainter& /*painter*/)
{
    // Controls are now QPushButton widgets, not painted
}

void MapWidget::drawAttribution(QPainter& painter)
{
    TileService* tileService = TileService::instance();
    QString attribution = tileService ? tileService->attribution() : QString();

    if (attribution.isEmpty())
    {
        return;
    }

    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);

    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(attribution);
    int padding = 4;
    int margin = 8;

    QRect bgRect(margin,
                 height() - textRect.height() - padding * 2 - margin,
                 textRect.width() + padding * 2,
                 textRect.height() + padding * 2);

    // Semi-transparent background
    painter.fillRect(bgRect, QColor(255, 255, 255, 200));

    // Text
    painter.setPen(QColor(100, 100, 100));
    painter.drawText(bgRect, Qt::AlignCenter, attribution);
}

// ============================================================================
// Mouse Handling
// ============================================================================

void MapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_isDragging = true;
        m_wasDragging = false;
        m_lastMousePos = event->pos();
        m_dragStartPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging)
    {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        // Check if we've dragged far enough to count as a drag (not a click)
        QPoint totalDelta = event->pos() - m_dragStartPos;
        if (qAbs(totalDelta.x()) > DRAG_THRESHOLD
            || qAbs(totalDelta.y()) > DRAG_THRESHOLD)
        {
            m_wasDragging = true;
            m_bounds.reset();  // User is manually panning, stop auto-fitting
        }

        // Stop any animation when user drags
        stopAnimation();

        // Convert pixel delta to lat/lng delta (use fractional zoom for smooth dragging)
        double clampedZoom = qBound(static_cast<double>(MIN_ZOOM), m_zoom, static_cast<double>(MAX_ZOOM));
        double lngPerPixel = SlippyMapMath::lngDegreesPerPixel(clampedZoom);
        double latPerPixel = SlippyMapMath::latDegreesPerPixel(clampedZoom, m_centerLat);

        m_centerLng -= delta.x() * lngPerPixel;
        m_centerLat += delta.y() * latPerPixel;

        // Clamp latitude
        m_centerLat = qBound(-85.0, m_centerLat, 85.0);

        // Wrap longitude
        while (m_centerLng < -180.0)
        {
            m_centerLng += 360.0;
        }
        while (m_centerLng > 180.0)
        {
            m_centerLng -= 360.0;
        }

        update();
        updateLayerButtonIcon();
    }
}

void MapWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_isDragging = false;
        setCursor(Qt::ArrowCursor);

        // If it was a click (not a drag), emit click signal
        // Empty string means clicked on empty map (providers can use to deselect)
        if (!m_wasDragging)
        {
            emit familyClicked(markerAtPoint(event->pos()));
        }
        else
        {
            // Update layer preview after drag ends
            updateLayerButtonIcon();
        }
    }
}

void MapWidget::wheelEvent(QWheelEvent* event)
{
    // Stop any animation when user scrolls
    stopAnimation();

    // Get mouse position at current (possibly fractional) zoom
    double clampedZoom = qBound(static_cast<double>(MIN_ZOOM), m_zoom, static_cast<double>(MAX_ZOOM));
    QPointF mouseLatLng = SlippyMapMath::pixelToLatLng(event->position(), clampedZoom,
                                                        m_centerLat, m_centerLng,
                                                        width(), height());
    double mouseLat = mouseLatLng.x();
    double mouseLng = mouseLatLng.y();

    int delta = event->angleDelta().y();
    int currentInt = static_cast<int>(qRound(m_zoom));
    int targetInt = currentInt;

    if (delta > 0 && targetInt < MAX_ZOOM)
    {
        targetInt++;
    }
    else if (delta < 0 && targetInt > MIN_ZOOM)
    {
        targetInt--;
    }

    if (targetInt != currentInt)
    {
        m_zoom = static_cast<double>(targetInt);
        m_bounds.reset();  // User is manually zooming, stop auto-fitting

        // Adjust center so mouse position stays fixed
        // After zoom, recalculate where the mouse lat/lng would appear
        // and adjust center to put it back under the mouse

        QPointF newMousePixel = SlippyMapMath::latLngToPixel(mouseLat, mouseLng, m_zoom,
                                                              m_centerLat, m_centerLng,
                                                              width(), height());
        double dx = event->position().x() - newMousePixel.x();
        double dy = event->position().y() - newMousePixel.y();

        double lngPerPixel = SlippyMapMath::lngDegreesPerPixel(m_zoom);
        double latPerPixel = SlippyMapMath::latDegreesPerPixel(m_zoom, m_centerLat);

        m_centerLng -= dx * lngPerPixel;
        m_centerLat += dy * latPerPixel;

        m_centerLat = qBound(-85.0, m_centerLat, 85.0);

        update();
        updateLayerButtonIcon();
    }

    event->accept();
}

QString MapWidget::markerAtPoint(const QPoint& pos) const
{
    const QVariantList& families = m_viewModel->families();
    double clampedZoom = qBound(static_cast<double>(MIN_ZOOM), m_zoom, static_cast<double>(MAX_ZOOM));

    // Check in reverse order (topmost markers drawn last)
    for (int i = families.size() - 1; i >= 0; --i)
    {
        QVariantMap hh = families[i].toMap();
        double lat = hh["latitude"].toDouble();
        double lng = hh["longitude"].toDouble();
        QPointF markerPos = SlippyMapMath::latLngToPixel(lat, lng, clampedZoom,
                                                          m_centerLat, m_centerLng,
                                                          width(), height());

        MarkerRenderer::State state;  // Default state for hit testing
        if (MarkerRenderer::hitTest(markerPos, pos, hh, state))
        {
            return hh["id"].toString();
        }
    }

    return QString();
}

// ============================================================================
// Button Handlers
// ============================================================================

void MapWidget::onZoomIn()
{
    double targetZoom = qFloor(m_zoom) + 1.0;
    if (targetZoom <= MAX_ZOOM)
    {
        animateTo(m_centerLat, m_centerLng, targetZoom);
    }
}

void MapWidget::onZoomOut()
{
    double targetZoom = qCeil(m_zoom) - 1.0;
    if (targetZoom >= MIN_ZOOM)
    {
        animateTo(m_centerLat, m_centerLng, targetZoom);
    }
}

void MapWidget::onToggleLayer()
{
    m_useSatellite = !m_useSatellite;
    updateLayerButtonIcon();
    update();
}

void MapWidget::onTileReady()
{
    update();
    updateLayerButtonIcon();  // Refresh preview when tiles load
}

// ============================================================================
// Public API
// ============================================================================

void MapWidget::ensureVisible(const QSet<QString>& familyIds)
{
    if (familyIds.isEmpty())
    {
        return;
    }

    // Compute bounds of all specified families
    const Document& doc = m_docManager->document();
    double minLat = 90.0, maxLat = -90.0;
    double minLng = 180.0, maxLng = -180.0;
    bool anyMapped = false;

    for (const QString& id : familyIds)
    {
        std::optional<Family> family = doc.findFamilyById(id);
        if (family && family->isMapped())
        {
            double lat = family->latitude().value();
            double lng = family->longitude().value();
            minLat = qMin(minLat, lat);
            maxLat = qMax(maxLat, lat);
            minLng = qMin(minLng, lng);
            maxLng = qMax(maxLng, lng);
            anyMapped = true;
        }
    }

    if (!anyMapped)
    {
        return;
    }

    // Check if highlighted bounds are within current viewport
    LatLngBounds currentViewport = viewportBounds(m_centerLat, m_centerLng, m_zoom);

    bool allVisible =
        minLat >= currentViewport.minLat
        && maxLat <= currentViewport.maxLat
        && minLng >= currentViewport.minLng
        && maxLng <= currentViewport.maxLng;

    if (allVisible)
    {
        return;
    }

    // Need to adjust view - compute target that shows all highlighted families
    double targetLat = (minLat + maxLat) / 2.0;
    double targetLng = (minLng + maxLng) / 2.0;

    // Calculate zoom needed to fit the highlighted bounds (never zoom in past current)
    LatLngBounds targetBounds{minLat, maxLat, minLng, maxLng};
    QMarginsF markerPadding = MarkerRenderer::boundingBox(QVariantMap(), MarkerRenderer::State());
    QMarginsF safeArea = calculateSafeAreaPadding();

    double availableWidth = width() - safeArea.left() - safeArea.right()
                            - markerPadding.left() - markerPadding.right();
    double availableHeight = height() - safeArea.top() - safeArea.bottom()
                             - markerPadding.top() - markerPadding.bottom();

    double latSpan = maxLat - minLat;
    double lngSpan = maxLng - minLng;

    double targetZoom = m_zoom;
    if (availableWidth > 0 && availableHeight > 0 && (lngSpan > 0 || latSpan > 0))
    {
        if (lngSpan > 0)
        {
            double zoomForLng = qLn(360.0 * availableWidth / (SlippyMapMath::TILE_SIZE * lngSpan)) / qLn(2.0);
            targetZoom = qMin(targetZoom, zoomForLng);
        }
        if (latSpan > 0)
        {
            double tileY1 = SlippyMapMath::latToTileY(minLat, 0);
            double tileY2 = SlippyMapMath::latToTileY(maxLat, 0);
            double tileYSpan = qAbs(tileY1 - tileY2);
            double zoomForLat = qLn(availableHeight / (tileYSpan * SlippyMapMath::TILE_SIZE)) / qLn(2.0);
            targetZoom = qMin(targetZoom, zoomForLat);
        }
        targetZoom = qBound(static_cast<double>(MIN_ZOOM), targetZoom, m_zoom);
    }

    // Clear bounds tracking since we're manually positioning
    m_bounds = std::nullopt;

    // Combine safe area and marker padding for animateTo
    QMarginsF contentPadding(
        safeArea.left() + markerPadding.left(),
        safeArea.top() + markerPadding.top(),
        safeArea.right() + markerPadding.right(),
        safeArea.bottom() + markerPadding.bottom()
    );
    animateTo(targetLat, targetLng, targetZoom, targetBounds, contentPadding);
}

void MapWidget::fitAllFamilies()
{
    const QVariantList& families = m_viewModel->families();

    if (families.isEmpty())
    {
        return;
    }

    double minLat = 90.0;
    double maxLat = -90.0;
    double minLng = 180.0;
    double maxLng = -180.0;

    for (const QVariant& var : families)
    {
        QVariantMap fam = var.toMap();
        double lat = fam["latitude"].toDouble();
        double lng = fam["longitude"].toDouble();

        minLat = qMin(minLat, lat);
        maxLat = qMax(maxLat, lat);
        minLng = qMin(minLng, lng);
        maxLng = qMax(maxLng, lng);
    }

    // Calculate target center
    double targetLat = (minLat + maxLat) / 2.0;
    double targetLng = (minLng + maxLng) / 2.0;

    // Store bounds with 5% padding
    double latPadding = (maxLat - minLat) * 0.05;
    double lngPadding = (maxLng - minLng) * 0.05;
    m_bounds = LatLngBounds{
        minLat - latPadding,
        maxLat + latPadding,
        minLng - lngPadding,
        maxLng + lngPadding
    };

    // Calculate target zoom for bounds
    double latRange = m_bounds->maxLat - m_bounds->minLat;
    double lngRange = m_bounds->maxLng - m_bounds->minLng;
    double targetZoom = MIN_ZOOM;

    // Get marker bounds and safe area for content padding
    MarkerRenderer::State markerState;
    QMarginsF markerPadding = MarkerRenderer::boundingBox(QVariantMap(), markerState);
    QMarginsF safeArea = calculateSafeAreaPadding();

    double availableWidth = width() - safeArea.left() - safeArea.right()
                            - markerPadding.left() - markerPadding.right();
    double availableHeight = height() - safeArea.top() - safeArea.bottom()
                             - markerPadding.top() - markerPadding.bottom();

    if (availableWidth > 0 && availableHeight > 0)
    {
        for (int z = MAX_ZOOM; z >= MIN_ZOOM; --z)
        {
            double degreesPerPixelLng = SlippyMapMath::lngDegreesPerPixel(z);
            double degreesPerPixelLat = SlippyMapMath::latDegreesPerPixel(z, targetLat);

            double visibleLngRange = availableWidth * degreesPerPixelLng;
            double visibleLatRange = availableHeight * degreesPerPixelLat;

            if (visibleLngRange >= lngRange && visibleLatRange >= latRange)
            {
                targetZoom = static_cast<double>(z);
                break;
            }
        }
    }

    // Combine safe area and marker padding for animateTo
    QMarginsF contentPadding(
        safeArea.left() + markerPadding.left(),
        safeArea.top() + markerPadding.top(),
        safeArea.right() + markerPadding.right(),
        safeArea.bottom() + markerPadding.bottom()
    );

    // Animate to target, passing family bounds so mid-zoom fits all markers
    animateTo(targetLat, targetLng, targetZoom, m_bounds, contentPadding);
}


void MapWidget::setCenter(double lat, double lng)
{
    m_centerLat = lat;
    m_centerLng = lng;
    update();
}

void MapWidget::setMarkerProvider(FamilyMarkerProvider* provider)
{
    m_markerProvider = provider;
    m_unmappedPanel->setMarkerProvider(provider);
    update();
}

void MapWidget::updateHighlights()
{
    if (m_markerProvider)
    {
        ensureVisible(m_markerProvider->highlightInfo().allHighlightedIds());
    }
    updateUnmappedPanelVisibility();
    update();
    emit highlightChanged();
}

// ============================================================================
// Animation
// ============================================================================

LatLngBounds MapWidget::viewportBounds(double centerLat, double centerLng, double zoom) const
{
    double halfWidth = width() / 2.0;
    double halfHeight = height() / 2.0;

    double lngPerPixel = SlippyMapMath::lngDegreesPerPixel(zoom);
    double latPerPixel = SlippyMapMath::latDegreesPerPixel(zoom, centerLat);

    return LatLngBounds{
        centerLat - halfHeight * latPerPixel,  // minLat (south)
        centerLat + halfHeight * latPerPixel,  // maxLat (north)
        centerLng - halfWidth * lngPerPixel,   // minLng (west)
        centerLng + halfWidth * lngPerPixel    // maxLng (east)
    };
}

QMarginsF MapWidget::calculateSafeAreaPadding() const
{
    // Left: zoom/recenter buttons (stacked vertically on left edge)
    double left = m_recenterButton->x() + m_recenterButton->width() + 10;

    // Top: small margin (buttons are on left, layer button on right - top-center is clear)
    double top = 20;

    // Right: unmapped panel width if visible and expanded, else layer button area
    // Note: panel grows upward first, so only its width affects right margin
    double right = (m_unmappedPanel->isVisible() && m_unmappedPanel->isExpanded())
        ? (width() - m_unmappedPanel->x() + 10)
        : (m_layerButton->width() + 20);

    // Bottom: attribution area in bottom-left (~30px)
    // Note: unmapped panel is bottom-right, only affects right margin not bottom
    double bottom = 30;

    return QMarginsF(left, top, right, bottom);
}

void MapWidget::animateTo(double lat, double lng, double zoom,
                          std::optional<LatLngBounds> targetBounds,
                          QMarginsF contentPadding)
{
    // Store start and target
    m_animStartLat = m_centerLat;
    m_animStartLng = m_centerLng;
    m_animStartZoom = m_zoom;
    m_animTargetLat = lat;
    m_animTargetLng = lng;
    m_animTargetZoom = qBound(static_cast<double>(MIN_ZOOM), zoom, static_cast<double>(MAX_ZOOM));
    m_animProgress = 0.0;

    // Calculate viewport bounds
    m_animStartBounds = viewportBounds(m_animStartLat, m_animStartLng, m_animStartZoom);
    // Use provided target bounds if given, otherwise calculate from viewport
    m_animTargetBounds = targetBounds.value_or(
        viewportBounds(m_animTargetLat, m_animTargetLng, m_animTargetZoom));

    // Check if target is already visible in current viewport
    bool targetVisible =
        m_animTargetBounds.minLat >= m_animStartBounds.minLat
        && m_animTargetBounds.maxLat <= m_animStartBounds.maxLat
        && m_animTargetBounds.minLng >= m_animStartBounds.minLng
        && m_animTargetBounds.maxLng <= m_animStartBounds.maxLng;

    // Calculate combined bounds: union of start viewport and target bounds
    // At mid-zoom, the full start viewport and target content must be visible
    double combinedMinLat = qMin(m_animStartBounds.minLat, m_animTargetBounds.minLat);
    double combinedMaxLat = qMax(m_animStartBounds.maxLat, m_animTargetBounds.maxLat);
    double combinedMinLng = qMin(m_animStartBounds.minLng, m_animTargetBounds.minLng);
    double combinedMaxLng = qMax(m_animStartBounds.maxLng, m_animTargetBounds.maxLng);

    double latSpan = combinedMaxLat - combinedMinLat;
    double lngSpan = combinedMaxLng - combinedMinLng;

    // Calculate safe area that avoids UI overlays (buttons, panels)
    QMarginsF safeArea = calculateSafeAreaPadding();

    // Available screen space = widget size minus UI overlays minus marker padding
    double availableWidth = width() - safeArea.left() - safeArea.right()
                            - contentPadding.left() - contentPadding.right();
    double availableHeight = height() - safeArea.top() - safeArea.bottom()
                             - contentPadding.top() - contentPadding.bottom();

    // Find exact fractional zoom level that fits this combined span
    // For longitude: at zoom z, degrees per pixel = 360 / (2^z * 256)
    // For latitude: use proper Mercator projection (non-linear)
    m_animMidZoom = MIN_ZOOM;
    if (availableWidth > 0 && availableHeight > 0 && lngSpan > 0 && latSpan > 0)
    {
        // Calculate zoom needed for longitude span
        double zoomForLng = qLn(360.0 * availableWidth / (SlippyMapMath::TILE_SIZE * lngSpan)) / qLn(2.0);

        // Calculate zoom needed for latitude span using proper Mercator math
        double tileY1 = SlippyMapMath::latToTileY(combinedMinLat, 0);
        double tileY2 = SlippyMapMath::latToTileY(combinedMaxLat, 0);
        double tileYSpan = qAbs(tileY1 - tileY2);

        double zoomForLat = qLn(availableHeight / (tileYSpan * SlippyMapMath::TILE_SIZE)) / qLn(2.0);

        // Use the more restrictive (lower) zoom to fit both dimensions
        m_animMidZoom = qMin(zoomForLng, zoomForLat);
        m_animMidZoom = qBound(static_cast<double>(MIN_ZOOM), m_animMidZoom, static_cast<double>(MAX_ZOOM));
    }

    // If target not already visible, zoom out to show both start and target
    if (!targetVisible)
    {
        // Calculate center that shows both start and target
        double combinedCenterLat = (combinedMinLat + combinedMaxLat) / 2.0;
        double combinedCenterLng = (combinedMinLng + combinedMaxLng) / 2.0;

        // Adjust center for asymmetric content padding (shift toward start side)
        double hOffset = (contentPadding.left() - contentPadding.right()) / 2.0;
        double vOffset = (contentPadding.top() - contentPadding.bottom()) / 2.0;

        double lngPerPixel = SlippyMapMath::lngDegreesPerPixel(m_animMidZoom);
        double latPerPixel = SlippyMapMath::latDegreesPerPixel(m_animMidZoom, combinedCenterLat);

        // Set target to the combined view (no zoom-in phase, just zoom out and stop)
        m_animTargetLng = combinedCenterLng + hOffset * lngPerPixel;
        m_animTargetLat = combinedCenterLat - vOffset * latPerPixel;
        m_animTargetZoom = m_animMidZoom;
    }

    // Start animation
    m_animationTimer->start();
}

void MapWidget::stopAnimation()
{
    m_animationTimer->stop();
    m_animProgress = 0.0;
}

double MapWidget::easeOutCubic(double t)
{
    return 1.0 - qPow(1.0 - t, 3.0);
}

void MapWidget::onAnimationTick()
{
    // Advance progress
    m_animProgress += static_cast<double>(ANIM_TICK_MS) / ANIM_DURATION_MS;

    if (m_animProgress >= 1.0)
    {
        // Animation complete - snap to final values
        m_centerLat = m_animTargetLat;
        m_centerLng = m_animTargetLng;
        m_zoom = m_animTargetZoom;
        stopAnimation();
        updateLayerButtonIcon();
    }
    else
    {
        // Animate zoom while keeping original viewport bounds visible
        double t = easeOutCubic(m_animProgress);

        // Interpolate zoom
        m_zoom = m_animStartZoom + (m_animTargetZoom - m_animStartZoom) * t;

        // Ideal center (linear interpolation toward target)
        double idealLat = m_animStartLat + (m_animTargetLat - m_animStartLat) * t;
        double idealLng = m_animStartLng + (m_animTargetLng - m_animStartLng) * t;

        // Calculate viewport half-dimensions at current zoom
        // Use start latitude for consistency with how start bounds were calculated
        double halfWidthLng = (width() / 2.0) * SlippyMapMath::lngDegreesPerPixel(m_zoom);
        double halfHeightLat = (height() / 2.0) * SlippyMapMath::latDegreesPerPixel(m_zoom, m_animStartLat);

        // Clamp center so original bounds stay fully visible
        // For south edge visible: startBounds.minLat >= center - halfHeight => center <= minLat + halfHeight
        // For north edge visible: startBounds.maxLat <= center + halfHeight => center >= maxLat - halfHeight
        double minCenterLat = m_animStartBounds.maxLat - halfHeightLat;
        double maxCenterLat = m_animStartBounds.minLat + halfHeightLat;
        double minCenterLng = m_animStartBounds.maxLng - halfWidthLng;
        double maxCenterLng = m_animStartBounds.minLng + halfWidthLng;

        // If viewport is too small to contain original bounds (min > max), use ideal center
        if (minCenterLat <= maxCenterLat)
        {
            m_centerLat = qBound(minCenterLat, idealLat, maxCenterLat);
        }
        else
        {
            m_centerLat = idealLat;
        }

        if (minCenterLng <= maxCenterLng)
        {
            m_centerLng = qBound(minCenterLng, idealLng, maxCenterLng);
        }
        else
        {
            m_centerLng = idealLng;
        }
    }

    update();
    updateLayerButtonIcon();
}
