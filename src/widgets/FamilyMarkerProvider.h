#pragma once

#include "Id.h"

#include <QSet>
#include <QString>

/// Semantic highlight data - providers specify WHAT to highlight,
/// MapWidget decides HOW to render it.
struct HighlightInfo
{
    QSet<FamilyId> highlightedFamilyIds;   // Families to highlight (glow + ring)
    QSet<FamilyId> contactPointFamilyIds;  // Families to highlight AND show pip (e.g., ministers)

    /// Returns true if any highlighting is active
    bool hasHighlighting() const
    {
        return !highlightedFamilyIds.isEmpty() || !contactPointFamilyIds.isEmpty();
    }

    /// Returns all families that should be highlighted (union of both sets)
    QSet<FamilyId> allHighlightedIds() const
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
    virtual QSet<FamilyId> visibleFamilyIds() const = 0;

    /// Optional status icon overlay (empty = no icon)
    /// Response Mode uses this for welfare status icons
    virtual QString familyStatusIcon(const FamilyId& familyId) const
    {
        Q_UNUSED(familyId);
        return {};
    }

    /// Clear any selection in this view (e.g., when map empty space is clicked)
    virtual void clearSelection() {}

    /// Select a family in this view (e.g., when a map marker is clicked)
    virtual void selectFamily(const FamilyId& familyId) { Q_UNUSED(familyId); }
};
