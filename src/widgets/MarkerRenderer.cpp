#include "MarkerRenderer.h"

#include "Document.h"
#include "Family.h"
#include "MarkerDecorationType.h"
#include "Person.h"

#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QIcon>
#include <QPainter>
#include <QSet>
#include <QtMath>

namespace MarkerRenderer
{

constexpr double MARKER_RADIUS = 10.0;

// File-local static icons (initialized on first use)
static QIcon homeIcon(":/markers/marker_home.svg");
static QIcon medicalIcon(":/markers/marker_medical.svg");
static QIcon recoveryIcon(":/markers/marker_recovery.svg");
static QIcon specialIcon(":/markers/marker_special.svg");
static QIcon medicalRecoveryIcon(":/markers/marker_medical_recovery.svg");
static QIcon specialMedicalIcon(":/markers/marker_special_medical.svg");
static QIcon specialRecoveryIcon(":/markers/marker_special_recovery.svg");
static QIcon specialMedicalRecoveryIcon(":/markers/marker_special_medical_recovery.svg");
static QIcon antennaIcon(":/markers/marker_antenna.svg");

// Select base icon from decoration set
static QIcon* selectBaseIcon(const QSet<MarkerDecorationType>& decorations)
{
    bool hasMedical = decorations.contains(MarkerDecorationType::Medical);
    bool hasRecovery = decorations.contains(MarkerDecorationType::Recovery);
    bool hasSpecialNeeds = decorations.contains(MarkerDecorationType::SpecialNeeds);

    if (hasSpecialNeeds && hasMedical && hasRecovery)
    {
        return &specialMedicalRecoveryIcon;
    }
    if (hasSpecialNeeds && hasMedical)
    {
        return &specialMedicalIcon;
    }
    if (hasSpecialNeeds && hasRecovery)
    {
        return &specialRecoveryIcon;
    }
    if (hasSpecialNeeds)
    {
        return &specialIcon;
    }
    if (hasMedical && hasRecovery)
    {
        return &medicalRecoveryIcon;
    }
    if (hasMedical)
    {
        return &medicalIcon;
    }
    if (hasRecovery)
    {
        return &recoveryIcon;
    }
    return &homeIcon;
}

MarkerIcons computeFamilyIcons(const QString& familyId, const Document& doc)
{
    QSet<MarkerDecorationType> decorations;
    const Family& family = doc.families().value(familyId);

    // Check each family member for special needs and skills
    for (const Person& person : family.members())
    {
        if (person.hasSpecialNeed())
        {
            decorations.insert(MarkerDecorationType::SpecialNeeds);
        }
        decorations.unite(doc.personSkillDecorations(person.id()));
    }

    // Equipment decorations (family-level)
    decorations.unite(doc.familyEquipmentDecorations(familyId));

    MarkerIcons icons;
    icons.baseIcon = selectBaseIcon(decorations);
    icons.antennaIcon = decorations.contains(MarkerDecorationType::Communications)
                        ? &antennaIcon : nullptr;
    return icons;
}

void draw(QPainter& painter, const QPointF& pos,
          const QVariantMap& family, const State& state)
{
    Q_UNUSED(family)  // For future use (resource decorations, etc.)

    double radius = MARKER_RADIUS * state.scale;
    double penWidth = state.scale;

    painter.setOpacity(state.opacity);

    if (state.isHighlighted)
    {
        // Ring dimensions (thin black ring)
        double ringWidth = 1.5 * state.scale;
        double ringOuterRadius = radius + ringWidth;

        // Flutter uses 3 stacked BoxShadows that composite together:
        // 1. blurRadius 14, spreadRadius 2, alpha 0.7
        // 2. blurRadius 8, spreadRadius 1, alpha 0.5
        // 3. blurRadius 6, offset (0,3), alpha 0.4

        // Helper lambda to create and draw a blurred shadow layer
        auto drawBlurredShadow = [&](double blurRad, double spreadRad, int alpha,
                                     double offsetX, double offsetY)
        {
            double effectiveRadius = ringOuterRadius + spreadRad * state.scale;
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

            QPointF drawPos(pos.x() - shadowSize / 2 + offsetX * state.scale,
                            pos.y() - shadowSize / 2 + offsetY * state.scale);
            painter.drawImage(drawPos, blurredImage);
        };

        // Draw all 3 shadow layers (order: largest blur first)
        // Reduced alpha from Flutter values since our markers are larger
        drawBlurredShadow(14.0 * state.scale, 2.0, 140, 0.0, 0.0);  // alpha ~0.55
        drawBlurredShadow(8.0 * state.scale, 1.0, 100, 0.0, 0.0);   // alpha ~0.4
        drawBlurredShadow(6.0 * state.scale, 0.0, 80, 0.0, 3.0);    // alpha ~0.3, offset down

        // Draw thin dark blue ring around the marker
        double ringRadius = radius + ringWidth / 2.0;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor("#1A3366"), ringWidth));  // Dark blue
        painter.drawEllipse(pos, ringRadius, ringRadius);

        // Draw marker circle (orange, no border - ring provides the edge)
        painter.setBrush(QColor("#F57C00"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(pos, radius, radius);
    }
    else
    {
        // Draw marker circle (orange with white border)
        painter.setBrush(QColor("#F57C00"));
        painter.setPen(QPen(Qt::white, 1.5 * penWidth));
        painter.drawEllipse(pos, radius, radius);
    }

    // Draw pip indicator for contact points
    if (state.showPip)
    {
        double pipRadius = 3.5 * state.scale;
        double pipOffset = radius * 0.7;  // Position at top-right of marker
        QPointF pipPos(pos.x() + pipOffset, pos.y() - pipOffset);

        // Pip with contrasting colors
        painter.setBrush(QColor("#2196F3"));  // Blue pip
        painter.setPen(QPen(Qt::white, 1.0 * penWidth));
        painter.drawEllipse(pipPos, pipRadius, pipRadius);
    }

    painter.setOpacity(1.0);
}

bool hitTest(const QPointF& markerPos, const QPointF& point,
             const QVariantMap& family, const State& state)
{
    Q_UNUSED(family)  // For future use (different hit areas per marker type)

    double dx = point.x() - markerPos.x();
    double dy = point.y() - markerPos.y();
    double distance = qSqrt(dx * dx + dy * dy);

    // Hit radius slightly larger than marker for easier clicking
    double hitRadius = (MARKER_RADIUS + 4) * state.scale;
    return distance <= hitRadius;
}

bool isVisible(const QPointF& pos, int widgetWidth, int widgetHeight)
{
    return pos.x() >= -MARKER_RADIUS
           && pos.x() <= widgetWidth + MARKER_RADIUS
           && pos.y() >= -MARKER_RADIUS
           && pos.y() <= widgetHeight + MARKER_RADIUS;
}

QMarginsF boundingBox(const QVariantMap& family, const State& state)
{
    Q_UNUSED(family)  // For future use (different marker types)

    double extent = MARKER_RADIUS * state.scale;

    // Highlighted markers have glow extending equally in all directions (ring + glow)
    double glowExtent = state.isHighlighted ? 23.0 * state.scale : 0.0;

    // Pip extends up-right from marker
    double pipExtent = state.showPip ? 3.5 * state.scale : 0.0;
    double pipOffset = state.showPip ? MARKER_RADIUS * 0.7 * state.scale : 0.0;

    // Calculate extent in each direction
    double left = extent + glowExtent;
    double top = extent + glowExtent + (state.showPip ? pipOffset + pipExtent : 0.0);
    double right = extent + glowExtent + (state.showPip ? pipOffset + pipExtent : 0.0);
    double bottom = extent + glowExtent;

    return QMarginsF(left, top, right, bottom);
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
