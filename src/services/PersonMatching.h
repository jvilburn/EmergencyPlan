#pragma once

#include "Family.h"

#include <QHash>
#include <optional>

/// Matches a household from an import to a family the ward holds, and its members to that
/// family's members. Contact details identify a household, since couples share a phone or an
/// email; names pick the person within it.
namespace PersonMatching
{
    /// The family in families that the household source is.
    ///
    /// The candidates are the families that share a contact key with source: the street line
    /// of the address, ignoring case and spacing, or a phone (its last ten digits) or an email
    /// of any member. With no candidate, the candidates are the families with source's surname
    /// whose members match at least half of source's. Among the candidates, the one matching
    /// the most of source's members wins, then the lowest id, so every import picks the same
    /// one. None when there is no candidate.
    std::optional<FamilyId> findFamily(const Family& source, const QHash<FamilyId, Family>& families);

    /// Which member of target each of source's members is, for those matched. Each member of
    /// target is matched at most once: first by the same given names, then by the same first
    /// given name, ignoring case. Among several, one whose gender differs where both are known
    /// is never taken, one with the same birthday day and month is preferred, and then the
    /// first in target's order.
    QHash<PersonId, PersonId> matchMembers(const Family& source, const Family& target);
}
