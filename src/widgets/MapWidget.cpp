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
#include <QPixmap>
#include <QPushButton>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QtMath>
#include <QDebug>
#include <QElapsedTimer>

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
    m_unmappedPanel = new UnmappedPanel(m_docManager, m_viewModel, this);
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
    QImage tile = tileService->getTile(TileId(otherLayer, tileZoom, tileX, tileY));

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
    QImage cropped = tile.copy(static_cast<int>(cropX), static_cast<int>(cropY),
                               cropSizeInt, cropSizeInt);
    QImage scaled = cropped.scaled(buttonIconSize, buttonIconSize,
                                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    m_layerButton->setIcon(QIcon(QPixmap::fromImage(scaled)));
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

    m_zoom = SlippyMapMath::zoomToFit(m_bounds->minLat, m_bounds->maxLat,
                                       m_bounds->minLng, m_bounds->maxLng,
                                       width(), height(), MIN_ZOOM, MAX_ZOOM);
}

// ============================================================================
// Painting
// ============================================================================

void MapWidget::paintEvent(QPaintEvent* /*event*/)
{
    QElapsedTimer totalTimer, phaseTimer;
    totalTimer.start();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    phaseTimer.start();
    drawTiles(painter);
    qint64 tilesUs = phaseTimer.nsecsElapsed() / 1000;

    phaseTimer.start();
    drawMarkers(painter);
    qint64 markersUs = phaseTimer.nsecsElapsed() / 1000;

    phaseTimer.start();
    drawChurchMarkers(painter);
    qint64 churchUs = phaseTimer.nsecsElapsed() / 1000;

    phaseTimer.start();
    drawAttribution(painter);
    qint64 attribUs = phaseTimer.nsecsElapsed() / 1000;

    qint64 totalUs = totalTimer.nsecsElapsed() / 1000;
    qDebug() << "paintEvent:" << totalUs << "us | tiles:" << tilesUs
             << "markers:" << markersUs << "church:" << churchUs
             << "attrib:" << attribUs;
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

    // Enable smooth scaling for quality when static, fast scaling when dragging
    bool needsScaling = scale != 1.0;
    bool useSmoothScaling = needsScaling && !m_isDragging;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, useSmoothScaling);

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
            QImage tile = tileService->getTile(TileId(layer, tileZoom, wrappedX, ty));

            // Draw the tile scaled
            QRectF targetRect(screenX, screenY, scaledTileSize, scaledTileSize);
            painter.drawImage(targetRect, tile);
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
            state.icons = m_viewModel->familyIcons(id);  // Use cached icons

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
        m_attributionRect = QRect();
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

    // Cache for collision detection
    m_attributionRect = bgRect;

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
        // Stop any animation immediately to prevent position changes during drag
        stopAnimation();

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

        QElapsedTimer dragTimer;
        dragTimer.start();
        update();
        qint64 updateUs = dragTimer.nsecsElapsed() / 1000;

        dragTimer.start();
        updateLayerButtonIcon();
        qint64 layerBtnUs = dragTimer.nsecsElapsed() / 1000;

        qDebug() << "mouseMoveEvent: update:" << updateUs << "us | layerBtn:" << layerBtnUs << "us";
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
            m_bounds = viewportBounds(m_centerLat, m_centerLng, m_zoom);
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

        m_bounds = viewportBounds(m_centerLat, m_centerLng, m_zoom);

        QElapsedTimer wheelTimer;
        wheelTimer.start();
        update();
        qint64 updateUs = wheelTimer.nsecsElapsed() / 1000;

        wheelTimer.start();
        updateLayerButtonIcon();
        qint64 layerBtnUs = wheelTimer.nsecsElapsed() / 1000;

        qDebug() << "wheelEvent: update:" << updateUs << "us | layerBtn:" << layerBtnUs << "us";
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

    // Collect visible markers + target markers
    QVector<MarkerInfo> markers;
    QSet<QString> addedIds;

    // Add currently visible family markers
    const QVariantList& allFamilies = m_viewModel->families();
    for (const QVariant& var : allFamilies)
    {
        QVariantMap fam = var.toMap();
        double lat = fam["latitude"].toDouble();
        double lng = fam["longitude"].toDouble();

        if (lat >= currentViewport.minLat && lat <= currentViewport.maxLat
            && lng >= currentViewport.minLng && lng <= currentViewport.maxLng)
        {
            QString id = fam["id"].toString();
            markers.append({lat, lng, MarkerRenderer::familyBounds(m_viewModel->familyIcons(id))});
            addedIds.insert(id);
        }
    }

    // Add target families (if not already added)
    for (const QString& id : familyIds)
    {
        if (addedIds.contains(id))
        {
            continue;
        }
        std::optional<Family> family = doc.findFamilyById(id);
        if (family && family->isMapped())
        {
            markers.append({
                family->latitude().value(),
                family->longitude().value(),
                MarkerRenderer::familyBounds(m_viewModel->familyIcons(id))
            });
        }
    }

    // Add chapel if currently visible
    const QHash<QString, Ward>& wards = doc.wards();
    for (const Ward& ward : wards)
    {
        if (ward.chapelLat() && ward.chapelLng())
        {
            double lat = ward.chapelLat().value();
            double lng = ward.chapelLng().value();
            if (lat >= currentViewport.minLat && lat <= currentViewport.maxLat
                && lng >= currentViewport.minLng && lng <= currentViewport.maxLng)
            {
                markers.append({lat, lng, MarkerRenderer::churchBounds()});
            }
        }
    }

    auto result = calculateAdaptiveZoom(markers);
    animateTo(result.lat, result.lng, result.zoom);
}

void MapWidget::fitAllFamilies()
{
    const Document& doc = m_docManager->document();
    const QVariantList& families = m_viewModel->families();

    QVector<MarkerInfo> markers;

    // Add all family markers
    for (const QVariant& var : families)
    {
        QVariantMap fam = var.toMap();
        QString id = fam["id"].toString();
        markers.append({
            fam["latitude"].toDouble(),
            fam["longitude"].toDouble(),
            MarkerRenderer::familyBounds(m_viewModel->familyIcons(id))
        });
    }

    // Add chapel if it has a location
    const QHash<QString, Ward>& wards = doc.wards();
    for (const Ward& ward : wards)
    {
        if (ward.chapelLat() && ward.chapelLng())
        {
            markers.append({
                ward.chapelLat().value(),
                ward.chapelLng().value(),
                MarkerRenderer::churchBounds()
            });
        }
    }

    if (markers.isEmpty())
    {
        return;
    }

    auto result = calculateAdaptiveZoom(markers);
    animateTo(result.lat, result.lng, result.zoom);
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

MapWidget::AdaptiveZoomResult MapWidget::calculateAdaptiveZoom(
    const QVector<MarkerInfo>& markers) const
{
    if (markers.isEmpty())
    {
        return {m_centerLat, m_centerLng, m_zoom};
    }

    // Calculate bounds and max padding from markers
    double minLat = 90.0, maxLat = -90.0;
    double minLng = 180.0, maxLng = -180.0;
    QMarginsF maxPadding;
    for (const MarkerInfo& m : markers)
    {
        minLat = qMin(minLat, m.lat);
        maxLat = qMax(maxLat, m.lat);
        minLng = qMin(minLng, m.lng);
        maxLng = qMax(maxLng, m.lng);
        maxPadding = QMarginsF(
            qMax(maxPadding.left(), m.bounds.left()),
            qMax(maxPadding.top(), m.bounds.top()),
            qMax(maxPadding.right(), m.bounds.right()),
            qMax(maxPadding.bottom(), m.bounds.bottom())
        );
    }

    double centerLat = (minLat + maxLat) / 2.0;
    double centerLng = (minLng + maxLng) / 2.0;

    // Build UI element rectangle for left button stack
    QRect leftButtonsRect(
        m_zoomInButton->x(),
        m_zoomInButton->y(),
        m_zoomInButton->width(),
        m_recenterButton->y() + m_recenterButton->height() - m_zoomInButton->y()
    );

    // Start with zoom that fits bounds with max marker padding
    double availableWidth = width() - maxPadding.left() - maxPadding.right();
    double availableHeight = height() - maxPadding.top() - maxPadding.bottom();
    double zoom = SlippyMapMath::zoomToFit(minLat, maxLat, minLng, maxLng,
                                            availableWidth, availableHeight, MIN_ZOOM, MAX_ZOOM);

    // Iterate until no new UI collisions (max 3 iterations to prevent infinite loops)
    QMarginsF safeArea;
    for (int i = 0; i < 3; ++i)
    {
        bool needsLeftButtons = false;
        bool needsLayerButton = false;
        bool needsAttribution = false;

        for (const MarkerInfo& m : markers)
        {
            QPointF pos = SlippyMapMath::latLngToPixel(m.lat, m.lng, zoom,
                                                        centerLat, centerLng, width(), height());
            QRectF markerRect(
                pos.x() - m.bounds.left(),
                pos.y() - m.bounds.top(),
                m.bounds.left() + m.bounds.right(),
                m.bounds.top() + m.bounds.bottom()
            );

            if (markerRect.intersects(leftButtonsRect))
            {
                needsLeftButtons = true;
            }
            if (markerRect.intersects(m_layerButton->geometry()))
            {
                needsLayerButton = true;
            }
            if (markerRect.intersects(m_attributionRect))
            {
                needsAttribution = true;
            }
        }

        QMarginsF newSafeArea(
            needsLeftButtons ? leftButtonsRect.right() : 0,
            0,
            needsLayerButton ? (width() - m_layerButton->x()) : 0,
            needsAttribution ? (height() - m_attributionRect.top()) : 0
        );

        // If no change in safe area, we're done
        if (newSafeArea == safeArea)
        {
            break;
        }

        safeArea = newSafeArea;

        // Recalculate zoom with new safe area
        availableWidth = width() - safeArea.left() - safeArea.right()
                         - maxPadding.left() - maxPadding.right();
        availableHeight = height() - safeArea.top() - safeArea.bottom()
                          - maxPadding.top() - maxPadding.bottom();
        zoom = SlippyMapMath::zoomToFit(minLat, maxLat, minLng, maxLng,
                                         availableWidth, availableHeight, MIN_ZOOM, MAX_ZOOM);
    }

    // Offset center to account for asymmetric safe areas
    // (content should be centered in safe area, not whole widget)
    double yOffsetPx = (safeArea.bottom() - safeArea.top()) / 2.0;
    double xOffsetPx = (safeArea.left() - safeArea.right()) / 2.0;
    centerLat -= yOffsetPx * SlippyMapMath::latDegreesPerPixel(zoom, centerLat);
    centerLng -= xOffsetPx * SlippyMapMath::lngDegreesPerPixel(zoom);

    return {centerLat, centerLng, zoom};
}

void MapWidget::animateTo(double lat, double lng, double zoom)
{
    m_animStartLat = m_centerLat;
    m_animStartLng = m_centerLng;
    m_animStartZoom = m_zoom;
    m_animTargetLat = lat;
    m_animTargetLng = lng;
    m_animTargetZoom = qBound(static_cast<double>(MIN_ZOOM), zoom, static_cast<double>(MAX_ZOOM));
    m_animProgress = 0.0;
    m_bounds = viewportBounds(lat, lng, m_animTargetZoom);
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
    m_animProgress += static_cast<double>(ANIM_TICK_MS) / ANIM_DURATION_MS;

    if (m_animProgress >= 1.0)
    {
        m_centerLat = m_animTargetLat;
        m_centerLng = m_animTargetLng;
        m_zoom = m_animTargetZoom;
        stopAnimation();
        updateLayerButtonIcon();
    }
    else
    {
        double t = easeOutCubic(m_animProgress);
        m_centerLat = m_animStartLat + (m_animTargetLat - m_animStartLat) * t;
        m_centerLng = m_animStartLng + (m_animTargetLng - m_animStartLng) * t;
        m_zoom = m_animStartZoom + (m_animTargetZoom - m_animStartZoom) * t;
    }

    update();
}
