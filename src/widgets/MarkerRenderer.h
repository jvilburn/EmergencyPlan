#pragma once

#include <QPainter>
#include <QPointF>
#include <QMarginsF>
#include <QColor>
#include <QVariantMap>
#include <QIcon>

class Ward;
class Document;

/// Renders map markers for families/persons.
namespace MarkerRenderer
{
    /// Pre-selected icons for a family marker (computed once, used during draw)
    struct MarkerIcons
    {
        QIcon* baseIcon = nullptr;     // Points to static icon (home, medical, etc.)
        QIcon* antennaIcon = nullptr;  // nullptr if no communications
    };

    /// State affecting marker appearance
    struct State
    {
        bool isHighlighted = false;
        bool showPip = false;  // Contact point indicator (small dot)
        double opacity = 1.0;
        double scale = 1.0;  // 0.5 for half-size markers
        QString statusIcon;  // For Response Mode welfare status overlay (e.g., "✓", "⚑", "?")
        MarkerIcons icons;   // Pre-selected marker icons
    };

    /// Compute marker icons for a family based on skills, equipment, and special needs.
    MarkerIcons computeFamilyIcons(const QString& familyId, const Document& doc);

    /// Draw a family marker at the given position.
    void draw(QPainter& painter, const QPointF& pos,
              const QVariantMap& family, const State& state);

    /// Check if a point is within a marker's hit area.
    bool hitTest(const QPointF& markerPos, const QPointF& point,
                 const QVariantMap& family, const State& state);

    /// Check if a marker at the given position is visible within widget bounds.
    bool isVisible(const QPointF& pos, int widgetWidth, int widgetHeight);

    /// Returns the bounding box as margins from the marker center point.
    /// This allows for markers that extend unevenly (e.g., labels, pins).
    QMarginsF boundingBox(const QVariantMap& family, const State& state);

    /// Draw a church/chapel marker at the given position.
    void drawChurch(QPainter& painter, const QPointF& pos, const State& state);

    /// Size of the church marker icon (for bounding calculations).
    constexpr double CHURCH_MARKER_SIZE = 24.0;
}
