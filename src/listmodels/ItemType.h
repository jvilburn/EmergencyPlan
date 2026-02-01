#pragma once

#include <QMetaType>

/// Unified item type enum for all tree models.
/// Each model uses a subset of these types.
enum class ItemType
{
    Invalid,
    ContactDetail,  // All models - expandable contact info

    // Emergency Resource types
    Resource,       // EmergencyResourceModel - resource group

    // Needs types
    Person,         // NeedsModel, EmergencyResourceModel - individual person

    // Ministering types
    District,           // MinisteringModel - top level grouping
    Companionship,      // MinisteringModel - minister group
    SectionHeader,      // MinisteringModel - "Ministers" / "Ministered" headers
    Minister,           // MinisteringModel - person who ministers
    MinisteredFamily,   // MinisteringModel, UnassignedModel - family being ministered
    MinisteredSister,   // MinisteringModel, UnassignedModel - sister being ministered (RS)
    UnassignedHeader    // UnassignedModel - "Unassigned (N)" header
};
Q_DECLARE_METATYPE(ItemType)
