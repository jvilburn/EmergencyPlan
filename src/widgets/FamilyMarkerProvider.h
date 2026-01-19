#pragma once

#include <QSet>
#include <QString>

/// Semantic highlight data - providers specify WHAT to highlight,
/// MapWidget decides HOW to render it.
struct HighlightInfo
{
    QSet<QString> highlightedFamilyIds;   // Families to highlight (glow + ring)
    QSet<QString> contactPointFamilyIds;  // Families to highlight AND show pip (e.g., ministers)

    /// Returns true if any highlighting is active
    bool hasHighlighting() const
    {
        return !highlightedFamilyIds.isEmpty() || !contactPointFamilyIds.isEmpty();
    }

    /// Returns all families that should be highlighted (union of both sets)
    QSet<QString> allHighlightedIds() const
    {
        return highlightedFamilyIds | contactPointFamilyIds;
    }
};

/// Interface for providing family marker state to MapWidget.
/// Sidebar views implement this to control how families appear on the map.
class FamilyMarkerProvider
{
public:
    virtual ~FamilyMarkerProvider() = default;

    /// Get highlight information - which families to emphasize and which are contact points
    virtual HighlightInfo highlightInfo() const = 0;

    /// Set of family IDs to show (empty = show all)
    virtual QSet<QString> visibleFamilyIds() const = 0;

    /// Optional status icon overlay (empty = no icon)
    /// Response Mode uses this for welfare status icons
    virtual QString familyStatusIcon(const QString& familyId) const
    {
        Q_UNUSED(familyId);
        return {};
    }
};
