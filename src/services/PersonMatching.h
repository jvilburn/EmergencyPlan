#pragma once

#include "Family.h"

#include <QString>
#include <QList>
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
}
