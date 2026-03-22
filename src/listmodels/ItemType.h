#pragma once

#include <QMetaType>

/// Unified item type enum for all tree models.
/// Each model uses a subset of these types.
enum class ItemType
{
    Invalid,
    ContactDetail,  // All models - expandable contact info
    Family,         // FamilyTreeModel - family group

    // Emergency Asset types
    Asset,       // EmergencyAssetModel - asset group

    // Team types
    Team,           // TeamsTreeModel - team group
    TeamMember,     // TeamsTreeModel - member of a team

    // Needs types
    Person,         // NeedsModel, EmergencyAssetModel - individual person

    // Ministering types
    District,           // MinisteringModel - top level grouping
    Companionship,      // MinisteringModel - minister group
    SectionHeader,      // MinisteringModel - "Ministers" / "Ministered" headers
    Minister,           // MinisteringModel - person who ministers
    MinisteredFamily,   // MinisteringModel, UnassignedModel - family being ministered
    MinisteredSister,   // MinisteringModel, UnassignedModel - sister being ministered (RS)
    UnassignedHeader,       // UnassignedModel - "Unassigned (N)" header
    TaskRow,                // TeamsTreeModel - task assigned to team
    UnassignedTasksHeader   // TeamsTreeModel - "Unassigned Tasks (N)" header
};
Q_DECLARE_METATYPE(ItemType)

/// Organization type for ministering views.
/// Used instead of bool to clarify intent at call sites.
enum class MinisteringOrg
{
    EldersQuorum,
    ReliefSociety
};
