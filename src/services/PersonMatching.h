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
