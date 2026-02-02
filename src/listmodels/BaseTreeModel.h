#pragma once

#include "ItemType.h"

#include <QAbstractItemModel>
#include <QSet>

/// Abstract base class for tree models that provide typed item access.
/// Inherits from QAbstractItemModel, adding three pure virtual methods.
/// Models inheriting from this can be used with SelectionPreservingTreeView.
///
/// Selection key formats by model (for reference):
///
///   FamilyTreeModel:
///     Family       -> {familyId}
///     Member       -> {personId}
///     Detail rows  -> parent's key
///
///   NeedsModel:
///     Need         -> {personId}
///     ContactDetail-> parent's key
///
///   EmergencyResourceModel:
///     Resource     -> {resourceId}
///     Person       -> {resourceId}:{personId}
///     ContactDetail-> parent's key
///
///   MinisteringModel:
///     District         -> {districtId}
///     Companionship    -> {groupId}
///     SectionHeader    -> {groupId}:ministers or {groupId}:ministered
///     Minister         -> {groupId}:minister:{personId}
///     MinisteredFamily -> {groupId}:family:{familyId}
///     MinisteredSister -> {groupId}:sister:{personId}
///     ContactDetail    -> parent's key
///
///   UnassignedMinisteringModel:
///     Header  -> "unassigned"
///     Family  -> {familyId}
///     Sister  -> {personId}
///
class BaseTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    using QAbstractItemModel::QAbstractItemModel;

    /// Returns the ItemType for the given index.
    /// Used by views for context menus, highlights, double-click behavior.
    virtual ItemType itemTypeAt(const QModelIndex& index) const = 0;

    /// Returns a unique selection key for the given index.
    /// Used by SelectionPreservingTreeView to save/restore selection.
    /// Keys must be unique within the model - use contextual format
    /// when the same entity can appear multiple times.
    /// Detail rows should return their parent's selection key.
    virtual QString selectionKeyAt(const QModelIndex& index) const = 0;

    /// Returns the entity ID for the given index.
    /// Used by views for highlights and commands.
    /// Returns empty string for invalid indices.
    virtual QString idAt(const QModelIndex& index) const = 0;
};

/// Domain data returned by models for family associations.
/// Views adapt this to HighlightInfo for the map.
struct FamilyAssociation
{
    QSet<QString> relatedFamilyIds;       // Families connected to this item
    QSet<QString> contactPointFamilyIds;  // Families of contact points (ministers, etc.)
};
