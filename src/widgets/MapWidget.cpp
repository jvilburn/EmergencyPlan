#include "MapWidget.h"
#include "MapHighlightProvider.h"
#include "SlippyMapMath.h"
#include "MarkerRenderer.h"
#include "UnmappedPanel.h"
#include "AppStyles.h"
#include "MapViewModel.h"
#include "DocumentManager.h"
#include "DocumentChange.h"
#include "TileService.h"
#include "Family.h"
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
    QString beveledStyle = AppStyles::beveledButtonCompact();

    // Zoom button style override - center the symbol with adjusted padding
    QString zoomStyle = beveledStyle
        + "QPushButton { color: #303030; padding: 0 0 2px 0; }";

    // Zoom in button
    m_zoomInButton = new QPushButton("+", this);
    m_zoomInButton->setFixedSize(32, 32);
    m_zoomInButton->setFont(QFont("Arial", 20, QFont::Black));
    m_zoomInButton->setCursor(Qt::PointingHandCursor);
    m_zoomInButton->setStyleSheet(zoomStyle);
    QGraphicsDropShadowEffect* zoomInShadow = new QGraphicsDropShadowEffect(m_zoomInButton);
    zoomInShadow->setBlurRadius(6);
    zoomInShadow->setOffset(0, 1);
    zoomInShadow->setColor(QColor(0, 0, 0, 60));
    m_zoomInButton->setGraphicsEffect(zoomInShadow);
    connect(m_zoomInButton, &QPushButton::clicked, this, &MapWidget::onZoomIn);

    // Zoom out button - use Unicode minus sign (wider than hyphen)
    m_zoomOutButton = new QPushButton(QChar(0x2212), this);
    m_zoomOutButton->setFixedSize(32, 32);
    m_zoomOutButton->setFont(QFont("Arial", 20, QFont::Black));
    m_zoomOutButton->setCursor(Qt::PointingHandCursor);
    m_zoomOutButton->setStyleSheet(zoomStyle);
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
    m_recenterButton->setStyleSheet(beveledStyle);
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
    m_layerButton->setStyleSheet(beveledStyle);
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
            this, [this](const QString& id) {
                m_selectedFamilyId = id;
                m_viewModel->selectFamily(id);
                update();
            });

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

    // Draw markers on top
    drawMarkers(painter);

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

void MapWidget::drawMarkers(QPainter& painter)
{
    const QVariantList& families = m_viewModel->families();

    // Use fractional zoom for smooth marker positioning during animation
    double clampedZoom = qBound(static_cast<double>(MIN_ZOOM), m_zoom, static_cast<double>(MAX_ZOOM));

    // If we have a highlight provider, use it for all coloring/opacity decisions
    if (m_highlightProvider)
    {
        QSet<QString> visibleIds = m_highlightProvider->visibleFamilyIds();
        bool hasVisibleFilter = !visibleIds.isEmpty();

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

            QColor color = m_highlightProvider->familyColor(id);
            qreal opacity = m_highlightProvider->familyOpacity(id);
            QString statusIcon = m_highlightProvider->familyStatusIcon(id);

            MarkerRenderer::State state;
            state.isSelected = (id == m_selectedFamilyId);
            state.isHighlighted = color.isValid();
            state.highlightColor = color.isValid() ? color : QColor("#4CAF50");
            state.opacity = opacity;
            state.statusIcon = statusIcon;

            markers.append({pos, hh, state, opacity});
        }

        // Sort by opacity so highlighted (opacity=1.0) markers draw on top
        std::sort(markers.begin(), markers.end(),
                  [](const MarkerInfo& a, const MarkerInfo& b)
                  {
                      return a.sortOpacity < b.sortOpacity;
                  });

        for (const MarkerInfo& m : markers)
        {
            MarkerRenderer::draw(painter, m.pos, m.data, m.state);
        }
        return;
    }

    // Legacy path: use m_highlightedIds and m_visibleIds directly
    bool hasHighlighting = !m_highlightedIds.isEmpty();
    bool hasFiltering = !m_visibleIds.isEmpty();

    // Draw non-highlighted markers first (dimmed if highlighting or filtering active)
    for (const QVariant& var : families)
    {
        QVariantMap hh = var.toMap();
        QString id = hh["id"].toString();

        if (m_highlightedIds.contains(id))
        {
            continue;  // Draw highlighted markers later, on top
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

        // Determine opacity based on highlighting and filtering
        bool isFilteredOut = hasFiltering && !m_visibleIds.contains(id);
        double opacity = 1.0;
        if (isFilteredOut)
        {
            opacity = 0.25;
        }
        else if (hasHighlighting)
        {
            opacity = 0.4;
        }

        MarkerRenderer::State state;
        state.isSelected = (id == m_selectedFamilyId);
        state.isHighlighted = false;
        state.opacity = opacity;

        MarkerRenderer::draw(painter, pos, hh, state);
    }

    // Draw highlighted markers on top
    for (const QVariant& var : families)
    {
        QVariantMap hh = var.toMap();
        QString id = hh["id"].toString();

        if (!m_highlightedIds.contains(id))
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

        QString colorStr = m_highlightedIds[id].toString();

        MarkerRenderer::State state;
        state.isSelected = (id == m_selectedFamilyId);
        state.isHighlighted = true;
        state.highlightColor = QColor(colorStr.isEmpty() ? "#4CAF50" : colorStr);
        state.opacity = 1.0;

        MarkerRenderer::draw(painter, pos, hh, state);
    }
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

        // If it was a click (not a drag), check for marker hit
        if (!m_wasDragging)
        {
            QString hitId = markerAtPoint(event->pos());
            if (!hitId.isEmpty())
            {
                m_selectedFamilyId = hitId;
                m_unmappedPanel->setSelectedFamilyId(hitId);
                m_viewModel->selectFamily(hitId);
                update();
            }
            else
            {
                // Clicked on empty map - deselect
                if (!m_selectedFamilyId.isEmpty())
                {
                    m_selectedFamilyId.clear();
                    m_unmappedPanel->setSelectedFamilyId(QString());
                    update();
                }
            }
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

void MapWidget::centerOnFamily(const QString& familyId)
{
    // Always update selection (for unmapped families too)
    m_selectedFamilyId = familyId;
    m_unmappedPanel->setSelectedFamilyId(familyId);
    update();

    const Document& doc = m_docManager->document();
    std::optional<Family> family = doc.findFamilyById(familyId);

    if (family && family->isMapped())
    {
        double targetLat = family->latitude().value();
        double targetLng = family->longitude().value();

        // Zoom in to at least 13.5 when centering on a family
        double targetZoom = qMax(m_zoom, 13.5);

        // Clear bounds tracking since we're manually positioning
        m_bounds = std::nullopt;

        // Pass target as a point (bounds where min=max)
        LatLngBounds targetPoint{targetLat, targetLat, targetLng, targetLng};

        // Get marker bounds for content padding so marker is fully visible at mid-zoom
        MarkerRenderer::State markerState;
        markerState.isSelected = true;
        QMarginsF contentPadding = MarkerRenderer::boundingBox(QVariantMap(), markerState);

        animateTo(targetLat, targetLng, targetZoom, targetPoint, contentPadding);
    }
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

    if (width() > 0 && height() > 0)
    {
        for (int z = MAX_ZOOM; z >= MIN_ZOOM; --z)
        {
            double degreesPerPixelLng = SlippyMapMath::lngDegreesPerPixel(z);
            double degreesPerPixelLat = SlippyMapMath::latDegreesPerPixel(z, targetLat);

            double widgetLngRange = width() * degreesPerPixelLng;
            double widgetLatRange = height() * degreesPerPixelLat;

            if (widgetLngRange >= lngRange && widgetLatRange >= latRange)
            {
                targetZoom = static_cast<double>(z);
                break;
            }
        }
    }

    // Get marker bounds for content padding so markers are fully visible
    MarkerRenderer::State markerState;
    QMarginsF contentPadding = MarkerRenderer::boundingBox(QVariantMap(), markerState);

    // Animate to target, passing family bounds so mid-zoom fits all markers
    animateTo(targetLat, targetLng, targetZoom, m_bounds, contentPadding);
}

void MapWidget::setHighlightedFamilies(const QVariantMap& ids)
{
    m_highlightedIds = ids;
    m_viewModel->setHighlighting(ids);
    update();
}

void MapWidget::clearHighlighting()
{
    m_highlightedIds.clear();
    m_viewModel->clearHighlighting();
    update();
}

void MapWidget::setCenter(double lat, double lng)
{
    m_centerLat = lat;
    m_centerLng = lng;
    update();
}

void MapWidget::setVisibleFamilyIds(const QStringList& ids)
{
    m_visibleIds = QSet<QString>(ids.begin(), ids.end());
    update();
}

void MapWidget::setHighlightProvider(MapHighlightProvider* provider)
{
    m_highlightProvider = provider;
    update();
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

    // Available screen space after content padding (for target marker visibility)
    double availableWidth = width() - contentPadding.left() - contentPadding.right();
    double availableHeight = height() - contentPadding.top() - contentPadding.bottom();

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

    // Determine if we need two-phase animation
    // Use two-phase if mid zoom is lower than both endpoints AND target not already visible
    double minEndpointZoom = qMin(m_animStartZoom, m_animTargetZoom);
    m_animTwoPhase = !targetVisible && (m_animMidZoom < minEndpointZoom - 0.5);

    // Calculate mid-point center as geographic center of combined bounds
    if (m_animTwoPhase)
    {
        double combinedCenterLat = (combinedMinLat + combinedMaxLat) / 2.0;
        double combinedCenterLng = (combinedMinLng + combinedMaxLng) / 2.0;

        // Adjust center for asymmetric content padding (shift toward start side)
        double hOffset = (contentPadding.left() - contentPadding.right()) / 2.0;
        double vOffset = (contentPadding.top() - contentPadding.bottom()) / 2.0;

        double lngPerPixel = SlippyMapMath::lngDegreesPerPixel(m_animMidZoom);
        double latPerPixel = SlippyMapMath::latDegreesPerPixel(m_animMidZoom, combinedCenterLat);

        m_animMidLng = combinedCenterLng + hOffset * lngPerPixel;
        m_animMidLat = combinedCenterLat - vOffset * latPerPixel;  // Negative: lat increases north
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

double MapWidget::easeInOutCubic(double t)
{
    return t < 0.5
        ? 4.0 * t * t * t
        : 1.0 - qPow(-2.0 * t + 2.0, 3.0) / 2.0;
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
    else if (m_animTwoPhase)
    {
        // Two-phase animation: zoom out keeping start visible, zoom in keeping target visible
        // Both phases slide along the geographic line between start and target

        // Calculate where start and target appear at mid-view
        QPointF startAtMid = SlippyMapMath::latLngToPixel(
            m_animStartLat, m_animStartLng, m_animMidZoom,
            m_animMidLat, m_animMidLng, width(), height());

        if (m_animProgress < 0.5)
        {
            // Phase 1: Zoom out, sliding start from center toward its mid-view position
            double p1 = easeOutCubic(m_animProgress * 2.0);

            m_zoom = m_animStartZoom + (m_animMidZoom - m_animStartZoom) * p1;

            // Slide start from screen center (p1=0) to its position at mid-view (p1=1)
            double startScreenX = width() / 2.0 + p1 * (startAtMid.x() - width() / 2.0);
            double startScreenY = height() / 2.0 + p1 * (startAtMid.y() - height() / 2.0);

            // Calculate center that puts startLat/Lng at startScreenX/Y
            double n = qPow(2.0, m_zoom);
            double startWorldX = ((m_animStartLng + 180.0) / 360.0) * n * SlippyMapMath::TILE_SIZE;
            double startWorldY = SlippyMapMath::latToTileY(m_animStartLat, m_zoom)
                                 * SlippyMapMath::TILE_SIZE;

            double centerWorldX = startWorldX - startScreenX + width() / 2.0;
            double centerWorldY = startWorldY - startScreenY + height() / 2.0;

            m_centerLng = centerWorldX / (n * SlippyMapMath::TILE_SIZE) * 360.0 - 180.0;
            m_centerLat = SlippyMapMath::tileYToLat(
                centerWorldY / SlippyMapMath::TILE_SIZE, m_zoom);
        }
        else
        {
            // Phase 2: Zoom in, sliding target from its mid-view position toward center
            double p2 = easeOutCubic((m_animProgress - 0.5) * 2.0);

            m_zoom = m_animMidZoom + (m_animTargetZoom - m_animMidZoom) * p2;

            // Where is target on screen at mid-view? That's our starting point.
            QPointF targetAtMid = SlippyMapMath::latLngToPixel(
                m_animTargetLat, m_animTargetLng, m_animMidZoom,
                m_animMidLat, m_animMidLng, width(), height());

            // Slide target from mid-view position (p2=0) to screen center (p2=1)
            double targetScreenX = targetAtMid.x() + p2 * (width() / 2.0 - targetAtMid.x());
            double targetScreenY = targetAtMid.y() + p2 * (height() / 2.0 - targetAtMid.y());

            // Calculate center that puts targetLat/Lng at targetScreenX/Y
            double n = qPow(2.0, m_zoom);
            double targetWorldX = ((m_animTargetLng + 180.0) / 360.0) * n * SlippyMapMath::TILE_SIZE;
            double targetWorldY = SlippyMapMath::latToTileY(m_animTargetLat, m_zoom)
                                  * SlippyMapMath::TILE_SIZE;

            double centerWorldX = targetWorldX - targetScreenX + width() / 2.0;
            double centerWorldY = targetWorldY - targetScreenY + height() / 2.0;

            m_centerLng = centerWorldX / (n * SlippyMapMath::TILE_SIZE) * 360.0 - 180.0;
            m_centerLat = SlippyMapMath::tileYToLat(
                centerWorldY / SlippyMapMath::TILE_SIZE, m_zoom);
        }
    }
    else
    {
        // Simple single-phase animation (target already visible or close by)
        double t = easeOutCubic(m_animProgress);
        m_centerLat = m_animStartLat + (m_animTargetLat - m_animStartLat) * t;
        m_centerLng = m_animStartLng + (m_animTargetLng - m_animStartLng) * t;
        m_zoom = m_animStartZoom + (m_animTargetZoom - m_animStartZoom) * t;
    }

    update();
    updateLayerButtonIcon();
}
