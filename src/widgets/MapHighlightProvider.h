#pragma once

#include <QColor>
#include <QSet>
#include <QString>

/// Interface for providing highlight information to MapWidget.
/// Sidebar views implement this to control how families appear on the map.
class MapHighlightProvider
{
public:
    virtual ~MapHighlightProvider() = default;

    /// Color for a family's map marker (default color if not highlighted)
    virtual QColor familyColor(const QString& familyId) const = 0;

    /// Opacity for a family's map marker (1.0 = full, 0.3 = dimmed)
    virtual qreal familyOpacity(const QString& familyId) const = 0;

    /// Set of family IDs to show (empty = show all)
    virtual QSet<QString> visibleFamilyIds() const = 0;

    /// Optional status icon overlay (empty = no icon)
    /// Response Mode uses this for welfare status icons
    virtual QString familyStatusIcon(const QString& familyId) const
    {
        Q_UNUSED(familyId);
        return {};
    }

    /// Whether this family should show a contact point pip indicator
    /// Used to distinguish ministers from families being ministered to
    virtual bool isContactPoint(const QString& familyId) const
    {
        Q_UNUSED(familyId);
        return false;
    }
};
