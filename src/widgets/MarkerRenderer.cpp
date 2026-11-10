#include "MarkerRenderer.h"

#include "Document.h"
#include "Family.h"
#include "ResponseArea.h"
#include "Person.h"

#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QSet>
#include <QtMath>

namespace MarkerRenderer
{

// Marker sizing constants
constexpr double SVG_BASE_SIZE = 24.0;

// Highlight ring and shadow parameters
constexpr double HIGHLIGHT_RING_WIDTH = 1.5;
constexpr double SHADOW_BLUR_RADIUS = 14.0;   // Largest blur layer
constexpr double SHADOW_SPREAD_RADIUS = 2.0;  // Largest spread layer
// Glow extent beyond marker radius (for bounding box calculations)
constexpr double HIGHLIGHT_GLOW_EXTENT = HIGHLIGHT_RING_WIDTH + SHADOW_SPREAD_RADIUS + SHADOW_BLUR_RADIUS;

// Antenna overlay positioning (relative to marker size)
constexpr double ANTENNA_SCALE = 0.8;
constexpr double ANTENNA_OFFSET_X = -0.01;  // Slight left offset to visually center antenna with marker
constexpr double ANTENNA_OFFSET_Y_BASE = -0.5;
constexpr double ANTENNA_OFFSET_Y_ADJUST = -0.89;

// Lazy-initialized icons (must be function-local statics to ensure QApplication exists)
static QIcon& homeIcon()
{
    static QIcon icon(":/markers/marker_home.svg");
    return icon;
}
static QIcon& medicalIcon()
{
    static QIcon icon(":/markers/marker_medical.svg");
    return icon;
}
static QIcon& recoveryIcon()
{
    static QIcon icon(":/markers/marker_recovery.svg");
    return icon;
}
static QIcon& specialIcon()
{
    static QIcon icon(":/markers/marker_special.svg");
    return icon;
}
static QIcon& medicalRecoveryIcon()
{
    static QIcon icon(":/markers/marker_medical_recovery.svg");
    return icon;
}
static QIcon& specialMedicalIcon()
{
    static QIcon icon(":/markers/marker_special_medical.svg");
    return icon;
}
static QIcon& specialRecoveryIcon()
{
    static QIcon icon(":/markers/marker_special_recovery.svg");
    return icon;
}
static QIcon& specialMedicalRecoveryIcon()
{
    static QIcon icon(":/markers/marker_special_medical_recovery.svg");
    return icon;
}
static QIcon& antennaIcon()
{
    static QIcon icon(":/markers/marker_antenna.svg");
    return icon;
}

// Select base icon from decoration set
static QIcon* selectBaseIcon(const QSet<ResponseArea>& decorations)
{
    bool hasMedical = decorations.contains(ResponseArea::Medical);
    bool hasRecovery = decorations.contains(ResponseArea::Recovery);
    bool hasSpecialNeeds = decorations.contains(ResponseArea::SpecialNeeds);

    if (hasSpecialNeeds && hasMedical && hasRecovery)
    {
        return &specialMedicalRecoveryIcon();
    }
    if (hasSpecialNeeds && hasMedical)
    {
        return &specialMedicalIcon();
    }
    if (hasSpecialNeeds && hasRecovery)
    {
        return &specialRecoveryIcon();
    }
    if (hasSpecialNeeds)
    {
        return &specialIcon();
    }
    if (hasMedical && hasRecovery)
    {
        return &medicalRecoveryIcon();
    }
    if (hasMedical)
    {
        return &medicalIcon();
    }
    if (hasRecovery)
    {
        return &recoveryIcon();
    }
    return &homeIcon();
}

MarkerIcons computeFamilyIcons(const FamilyId& familyId, const Document& doc)
{
    QSet<ResponseArea> decorations;
    const Family& family = doc.families().value(familyId);

    // Check each family member for special needs and emergency asset assignments
    for (const Person& person : family.members())
    {
        if (person.hasSpecialNeed())
        {
            decorations.insert(ResponseArea::SpecialNeeds);
        }
        decorations.unite(doc.personResponseAreas(person.id()));
    }

    // Pre-render pixmaps at standard size to avoid repeated QIcon::pixmap() calls
    constexpr int standardSize = static_cast<int>(SVG_BASE_SIZE);
    QIcon* baseIcon = selectBaseIcon(decorations);
    bool hasCommunications = decorations.contains(ResponseArea::Communications);

    MarkerIcons icons;
    icons.basePixmap = baseIcon->pixmap(QSize(standardSize, standardSize));
    icons.hasAntenna = hasCommunications;
    if (hasCommunications)
    {
        icons.antennaPixmap = antennaIcon().pixmap(QSize(standardSize, standardSize));
    }
    return icons;
}

// Helper to draw a blurred shadow layer for highlighted markers
static void drawBlurredShadow(QPainter& painter, const QPointF& pos,
                               double ringOuterRadius, double scale,
                               double blurRad, double spreadRad, int alpha,
                               double offsetX, double offsetY)
{
    double effectiveRadius = ringOuterRadius + spreadRad * scale;
    double shadowSize = (effectiveRadius + blurRad) * 2 + 4;
    QImage shadowImage(static_cast<int>(shadowSize), static_cast<int>(shadowSize),
                       QImage::Format_ARGB32_Premultiplied);
    shadowImage.fill(Qt::transparent);

    QPainter shadowPainter(&shadowImage);
    shadowPainter.setRenderHint(QPainter::Antialiasing);
    shadowPainter.setBrush(QColor(0, 0, 0, alpha));
    shadowPainter.setPen(Qt::NoPen);
    QPointF shadowCenter(shadowSize / 2, shadowSize / 2);
    shadowPainter.drawEllipse(shadowCenter, effectiveRadius, effectiveRadius);
    shadowPainter.end();

    QGraphicsScene scene;
    QGraphicsPixmapItem* item = scene.addPixmap(QPixmap::fromImage(shadowImage));
    QGraphicsBlurEffect* blur = new QGraphicsBlurEffect();
    blur->setBlurRadius(blurRad);
    blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
    item->setGraphicsEffect(blur);

    QImage blurredImage(shadowImage.size(), QImage::Format_ARGB32_Premultiplied);
    blurredImage.fill(Qt::transparent);
    QPainter blurPainter(&blurredImage);
    scene.render(&blurPainter);
    blurPainter.end();

    QPointF drawPos(pos.x() - shadowSize / 2 + offsetX * scale,
                    pos.y() - shadowSize / 2 + offsetY * scale);
    painter.drawImage(drawPos, blurredImage);
}

// The glow around a highlighted marker: three blurred shadow layers centered in the image.
// It depends only on scale, so each scale's glow is rendered once and kept.
static QImage highlightGlow(double scale)
{
    static QHash<double, QImage> glows;
    auto it = glows.constFind(scale);
    if (it != glows.constEnd())
    {
        return *it;
    }

    double ringOuterRadius = (SVG_BASE_SIZE / 2.0 + HIGHLIGHT_RING_WIDTH) * scale;
    // The largest layer's reach, plus the third layer's 3-point drop and a margin
    double extent = ringOuterRadius + (SHADOW_SPREAD_RADIUS + SHADOW_BLUR_RADIUS + 3.0) * scale + 2.0;
    int canvasSize = static_cast<int>(qCeil(extent * 2.0));
    QImage glow(canvasSize, canvasSize, QImage::Format_ARGB32_Premultiplied);
    glow.fill(Qt::transparent);

    QPainter painter(&glow);
    QPointF center(canvasSize / 2.0, canvasSize / 2.0);
    // Largest blur first
    drawBlurredShadow(painter, center, ringOuterRadius, scale,
                      SHADOW_BLUR_RADIUS * scale, SHADOW_SPREAD_RADIUS, 140, 0.0, 0.0);
    drawBlurredShadow(painter, center, ringOuterRadius, scale,
                      8.0 * scale, 1.0, 100, 0.0, 0.0);
    drawBlurredShadow(painter, center, ringOuterRadius, scale,
                      6.0 * scale, 0.0, 80, 0.0, 3.0);
    painter.end();

    glows.insert(scale, glow);
    return glow;
}

void draw(QPainter& painter, const QPointF& pos, const State& state)
{
    double size = SVG_BASE_SIZE * state.scale;
    int intSize = static_cast<int>(qCeil(size));

    painter.setOpacity(state.opacity);

    if (state.isHighlighted)
    {
        // Ring dimensions (thin ring around the marker)
        double ringWidth = HIGHLIGHT_RING_WIDTH * state.scale;
        double markerRadius = size / 2.0;

        QImage glow = highlightGlow(state.scale);
        painter.drawImage(QPointF(pos.x() - glow.width() / 2.0, pos.y() - glow.height() / 2.0), glow);

        // Draw thin dark blue ring around the marker
        double ringRadius = markerRadius + ringWidth / 2.0;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor("#1A3366"), ringWidth));
        painter.drawEllipse(pos, ringRadius, ringRadius);
    }

    // Draw antenna behind marker if present
    if (state.icons.hasAntenna && !state.icons.antennaPixmap.isNull())
    {
        double antennaSize = size * ANTENNA_SCALE;
        double dpr = state.icons.antennaPixmap.devicePixelRatio();
        int antennaPhysSize = static_cast<int>(qCeil(antennaSize * dpr));
        QPixmap antennaPixmap = state.icons.antennaPixmap.scaled(
            antennaPhysSize, antennaPhysSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        antennaPixmap.setDevicePixelRatio(dpr);

        // Position antenna centered above marker (using logical size)
        QPointF antennaPos(pos.x() - antennaSize / 2.0 + size * ANTENNA_OFFSET_X,
                          pos.y() + size * ANTENNA_OFFSET_Y_BASE + antennaSize * ANTENNA_OFFSET_Y_ADJUST);
        painter.drawPixmap(antennaPos, antennaPixmap);
    }

    // Draw the marker icon using pre-rendered pixmap
    QPixmap pixmap = state.icons.basePixmap;
    if (pixmap.isNull())
    {
        // Fallback to home icon if no pixmap cached
        pixmap = homeIcon().pixmap(QSize(intSize, intSize));
    }
    else if (state.scale != 1.0)
    {
        // Scale the pre-rendered pixmap for non-standard sizes
        pixmap = pixmap.scaled(intSize, intSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QPointF topLeft(pos.x() - size / 2.0, pos.y() - size / 2.0);
    painter.drawPixmap(topLeft, pixmap);

    // Draw pip indicator for contact points
    if (state.showPip)
    {
        double pipRadius = 3.5 * state.scale;
        double pipOffset = (size / 2.0) * 0.7;
        QPointF pipPos(pos.x() + pipOffset, pos.y() - pipOffset);

        painter.setBrush(QColor("#2196F3"));
        painter.setPen(QPen(Qt::white, 1.0 * state.scale));
        painter.drawEllipse(pipPos, pipRadius, pipRadius);
    }

    // Draw welfare status badge (bottom-right corner)
    if (!state.statusIcon.isEmpty())
    {
        double badgeRadius = 5.0 * state.scale;
        double badgeOffset = (size / 2.0) * 0.6;
        QPointF badgePos(pos.x() + badgeOffset, pos.y() + badgeOffset);

        QColor badgeBg;
        if (state.statusIcon == STATUS_OK)
        {
            badgeBg = QColor(76, 175, 80);    // Green
        }
        else if (state.statusIcon == STATUS_NEEDS_HELP)
        {
            badgeBg = QColor(255, 152, 0);    // Orange
        }
        else if (state.statusIcon == STATUS_UNABLE_TO_REACH)
        {
            badgeBg = QColor(255, 235, 59);   // Yellow
        }

        if (badgeBg.isValid())
        {
            // Draw badge circle with white outline
            painter.setBrush(badgeBg);
            painter.setPen(QPen(Qt::white, 1.5 * state.scale));
            painter.drawEllipse(badgePos, badgeRadius, badgeRadius);

            // Draw symbol (dark text on yellow for contrast)
            bool isYellow = (state.statusIcon == STATUS_UNABLE_TO_REACH);
            painter.setPen(isYellow ? QColor(50, 50, 50) : QColor(Qt::white));
            QFont origFont = painter.font();
            QFont badgeFont = origFont;
            badgeFont.setPixelSize(static_cast<int>(8.0 * state.scale));
            badgeFont.setBold(true);
            painter.setFont(badgeFont);
            painter.drawText(QRectF(badgePos.x() - badgeRadius, badgePos.y() - badgeRadius,
                                     badgeRadius * 2, badgeRadius * 2),
                              Qt::AlignCenter, state.statusIcon);
            painter.setFont(origFont);
        }
    }

    painter.setOpacity(1.0);
}

bool hitTest(const QPointF& markerPos, const QPointF& point, const State& state)
{
    double dx = point.x() - markerPos.x();
    double dy = point.y() - markerPos.y();
    double distance = qSqrt(dx * dx + dy * dy);

    // Hit radius slightly larger than marker for easier clicking
    double hitRadius = ((SVG_BASE_SIZE / 2.0) + 4) * state.scale;
    return distance <= hitRadius;
}

bool isVisible(const QPointF& pos, int widgetWidth, int widgetHeight)
{
    double margin = SVG_BASE_SIZE / 2.0;
    return pos.x() >= -margin
           && pos.x() <= widgetWidth + margin
           && pos.y() >= -margin
           && pos.y() <= widgetHeight + margin;
}

QMarginsF familyBounds(const MarkerIcons& icons)
{
    // Compute bounds assuming highlighted (for zoom-to-fit, target will be selected)
    double markerRadius = SVG_BASE_SIZE / 2.0;
    double antennaExtent = icons.hasAntenna ? SVG_BASE_SIZE * ANTENNA_SCALE : 0.0;

    double left = markerRadius + HIGHLIGHT_GLOW_EXTENT;
    double top = markerRadius + HIGHLIGHT_GLOW_EXTENT + antennaExtent;
    double right = markerRadius + HIGHLIGHT_GLOW_EXTENT + antennaExtent;
    double bottom = markerRadius + HIGHLIGHT_GLOW_EXTENT;

    return QMarginsF(left, top, right, bottom);
}

QMarginsF churchBounds()
{
    // Church marker is a simple centered icon with no glow
    double radius = CHURCH_MARKER_SIZE / 2.0;
    return QMarginsF(radius, radius, radius, radius);
}

void drawChurch(QPainter& painter, const QPointF& pos, const State& state)
{
    static QIcon chapelIcon(":/markers/marker_chapel.svg");

    double size = CHURCH_MARKER_SIZE * state.scale;
    int intSize = static_cast<int>(qCeil(size));

    // Get pixmap at the needed size
    QPixmap pixmap = chapelIcon.pixmap(QSize(intSize, intSize));

    painter.setOpacity(state.opacity);

    // Draw centered on position
    QPointF topLeft(pos.x() - size / 2.0, pos.y() - size / 2.0);
    painter.drawPixmap(topLeft, pixmap);

    painter.setOpacity(1.0);
}

}  // namespace MarkerRenderer
