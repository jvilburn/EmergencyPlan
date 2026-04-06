#pragma once

#include "Id.h"

#include <QColor>
#include <QMarginsF>
#include <QPainter>
#include <QPixmap>
#include <QPointF>
#include <QVariantMap>

class Ward;
class Document;

/// Renders map markers for families/persons.
namespace MarkerRenderer
{
    /// Pre-rendered pixmaps for a family marker (computed once, used during draw)
    struct MarkerIcons
    {
        QPixmap basePixmap;      // Pre-rendered at standard size (24px)
        QPixmap antennaPixmap;   // Empty if no communications
        bool hasAntenna = false; // For bounds calculation
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
    MarkerIcons computeFamilyIcons(const FamilyId& familyId, const Document& doc);

    /// Draw a family marker at the given position.
    void draw(QPainter& painter, const QPointF& pos,
              const QVariantMap& family, const State& state);

    /// Check if a point is within a marker's hit area.
    bool hitTest(const QPointF& markerPos, const QPointF& point,
                 const QVariantMap& family, const State& state);

    /// Check if a marker at the given position is visible within widget bounds.
    bool isVisible(const QPointF& pos, int widgetWidth, int widgetHeight);

    /// Returns bounding box for a family marker (highlighted, with given icons).
    /// Used for zoom-to-fit calculations.
    QMarginsF familyBounds(const MarkerIcons& icons);

    /// Returns bounding box for a church/chapel marker.
    /// Used for zoom-to-fit calculations.
    QMarginsF churchBounds();

    /// Draw a church/chapel marker at the given position.
    void drawChurch(QPainter& painter, const QPointF& pos, const State& state);

    /// Size of the church marker icon (for bounding calculations).
    constexpr double CHURCH_MARKER_SIZE = 24.0;

    /// Status icon strings for welfare check badges.
    /// Used by FamilyMarkerProvider::familyStatusIcon() and badge rendering.
    inline const QString STATUS_OK = QString::fromUtf8("\xe2\x9c\x93");           // ✓
    inline const QString STATUS_NEEDS_HELP = QString::fromUtf8("\xe2\x9a\x91");   // ⚑
    inline const QString STATUS_UNABLE_TO_REACH = "?";
}
