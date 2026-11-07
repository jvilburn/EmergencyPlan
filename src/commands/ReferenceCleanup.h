#pragma once

#include "EmergencyAsset.h"
#include "EmergencyResponse.h"
#include "Id.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "Team.h"

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

class Document;

/// What a removal takes out of the places that refer to what it removes, and the places it
/// found when it was built, for its undo.
///
/// It removes people and families, or a team. The places that refer to a person are team
/// memberships and leaders, assets, ministering groups (ministers, the people ministered to
/// and the presidency member) and districts (the presidency member), and task assignments;
/// to a family, ministering groups' families; to a team, task assignments. The places are
/// held as the team, asset, group, district or task stood when the cleanup was built.
class ReferenceCleanup
{
public:
    /// The cleanup that removes people and families, holding each place in document that
    /// refers to one of them.
    static ReferenceCleanup of(const Document& document, const QSet<PersonId>& people, const QSet<FamilyId>& families);

    /// The cleanup that removes team, holding each task in document assigned to it.
    static ReferenceCleanup ofTeam(const Document& document, const TeamId& team);

    /// Takes the removed ids out of every place in document that holds them now, whatever
    /// the places it holds: each removed person leaves every team, asset and group, and every
    /// leader and presidency role they fill is cleared; each removed family leaves every
    /// group; every task assigned to a removed person or team is unassigned.
    void apply(Document& document) const;

    /// Puts the removed ids back into each held place that still stands as apply left it:
    /// the team, asset, group or district is there; the leader or presidency role is empty
    /// or already theirs; the task is there and unassigned or already assigned as held. It
    /// leaves the rest.
    void restore(Document& document) const;

    /// A line per held place restore would leave on document, naming it as document holds
    /// it, or as last seen when it is gone, in sorted order.
    QStringList skippedOn(const Document& document) const;

    /// True when it holds at least one place.
    bool holdsPlaces() const;

    QJsonObject toJson() const;
    static ReferenceCleanup fromJson(const QJsonObject& json);

    bool operator==(const ReferenceCleanup& other) const;
    bool operator!=(const ReferenceCleanup& other) const { return !(*this == other); }

private:
    ReferenceCleanup() = default;

    /// Restores as restore does, and answers the lines skippedOn gives.
    QStringList restoreInto(Document& document) const;

    bool namesRemoved(const std::optional<PersonId>& id) const;
    bool namesRemoved(const std::optional<TaskAssignment>& assignment) const;

    QSet<PersonId> m_people;
    QSet<FamilyId> m_families;
    QSet<TeamId> m_teams;

    QHash<TeamId, Team> m_heldTeams;
    QHash<EmergencyAssetId, EmergencyAsset> m_heldAssets;
    QHash<MinisteringGroupId, MinisteringGroup> m_heldGroups;
    QHash<MinisteringDistrictId, MinisteringDistrict> m_heldDistricts;
    QHash<TaskId, ResponseTask> m_heldTasks;
};
