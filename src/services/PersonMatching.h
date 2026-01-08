#pragma once

#include "Family.h"

#include <QHash>
#include <QList>
#include <QString>
#include <optional>

namespace PersonMatching
{
    /// Result of a family match with confidence score
    struct FamilyMatchResult
    {
        std::optional<Family> family;
        int score = 0;  // Higher = more confident match
    };

    /// Score for a person match
    struct PersonMatchScore
    {
        QString personId;
        int score = 0;
    };

    /// Result of family matching with member-based scoring
    struct FamilyMemberMatchResult
    {
        QString familyId;           // Empty if no match
        int matchedMembers = 0;     // How many members matched
        int totalSourceMembers = 0; // How many members in source family
        QHash<QString, QString> personIdMapping;  // source person ID -> target person ID
    };

    /// Result of searching for family members across all families
    struct FamilyReplacementResult
    {
        QString replacedFamilyId;   // The family being replaced (empty if truly new)
        int matchedMembers = 0;     // How many source members found in that family
        int totalSourceMembers = 0;
        QHash<QString, QString> personIdMapping;  // source person ID -> existing person ID
    };

    /// Score how well two persons match.
    /// Disqualifiers: no first name match, gender mismatch (if both have gender).
    /// Scoring: firstName exact +10, firstName partial +5, birth month+day +7,
    /// both isParent +2, phone match +5, email match +5.
    int scorePersonMatch(const Person& source, const Person& target);

    /// Find the best matching person in a family using scored matching.
    /// Returns the person ID and score. Score of 0 means no match.
    PersonMatchScore findBestPersonMatch(const Person& source, const Family& family);

    /// Find the best matching family from a list of candidates.
    /// Multi-factor scoring: address +100, phone +50, member +20 each.
    /// Returns nullopt if no match meets minimum threshold (40 points).
    FamilyMatchResult findBestFamilyMatch(
        const QString& familyName,
        const QString& address,
        const QString& phone,
        const QList<Family>& existingFamilies);

    /// Find a person by name across a list of families.
    /// Returns the first match found.
    std::optional<Person> findPersonInFamilies(
        const QString& personName,
        const QList<Family>& families);

    /// Find a matching family using member-based matching.
    /// Filters by surname first, then checks if majority of source members match.
    /// Returns empty familyId if no match with majority.
    FamilyMemberMatchResult findFamilyByMembers(
        const Family& sourceFamily,
        const QHash<QString, Family>& targetFamilies);

    /// Search all families for members of the source family (ignoring surname).
    /// Used as fallback when surname-based matching fails.
    /// Returns the family that contains at least half of source members, if any.
    FamilyReplacementResult findReplacedFamily(
        const Family& sourceFamily,
        const QHash<QString, Family>& targetFamilies);
}
