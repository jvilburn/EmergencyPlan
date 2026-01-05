#include "MarkerRenderer.h"

#include <QtMath>

namespace MarkerRenderer
{

constexpr double MARKER_RADIUS = 13.0;

void draw(QPainter& painter, const QPointF& pos,
          const QVariantMap& family, const State& state)
{
    Q_UNUSED(family)  // For future use (resource decorations, etc.)

    double radius = MARKER_RADIUS * state.scale;
    double ringOffset = 4.0 * state.scale;
    double penWidth = state.scale;

    painter.setOpacity(state.opacity);

    if (state.isHighlighted)
    {
        // Draw glow/shadow
        painter.setBrush(QColor(0, 0, 0, 64));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(pos, radius + ringOffset, radius + ringOffset);

        // Draw highlight ring
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(state.highlightColor, 3.0 * penWidth));
        painter.setOpacity(state.opacity * 0.6);
        painter.drawEllipse(pos, radius + ringOffset, radius + ringOffset);
        painter.setOpacity(state.opacity);

        // Draw marker circle
        QColor fillColor = state.isSelected ? QColor("#1976D2") : QColor("#F57C00");
        painter.setBrush(fillColor);
        painter.setPen(QPen(state.highlightColor, 3.0 * penWidth));
        painter.drawEllipse(pos, radius, radius);
    }
    else
    {
        // Draw shadow (if selected)
        if (state.isSelected)
        {
            painter.setBrush(QColor(0, 0, 0, 64));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(pos, radius + 2.0 * state.scale, radius + 2.0 * state.scale);
        }

        // Draw marker circle
        QColor fillColor = state.isSelected ? QColor("#1976D2") : QColor("#F57C00");
        QColor borderColor = state.isSelected ? QColor("#0D47A1") : Qt::white;

        painter.setBrush(fillColor);
        painter.setPen(QPen(borderColor, (state.isSelected ? 3.0 : 1.5) * penWidth));
        painter.drawEllipse(pos, radius, radius);
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

    // Hit radius includes highlight ring, scaled
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

    // Current markers are circular, but highlighted/selected ones have a larger ring
    double extent = MARKER_RADIUS * state.scale;
    if (state.isHighlighted || state.isSelected)
    {
        extent = (MARKER_RADIUS + 4) * state.scale;  // Includes glow/highlight ring
    }

    // For circular markers, all sides are equal
    // Future: pins might have more top extent, labels more right extent, etc.
    return QMarginsF(extent, extent, extent, extent);
}

}  // namespace MarkerRenderer
