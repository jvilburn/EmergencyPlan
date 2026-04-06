#pragma once

#include <QString>

/// Type-safe wrapper for names stored in "Surname, GivenNames" format.
/// Inherits from QString for seamless interoperability.
struct Name : public QString
{
public:
    using QString::QString;

    /// Construct from separate parts, storing as "Surname," or "Surname, GivenNames".
    Name(const QString& surname, const QString& givenNames);

    /// Extract surname (part before comma, or entire string if no comma).
    QString surname() const;

    /// Extract given names (part after comma, or empty if no comma).
    QString givenNames() const;

    /// Extract first name only (first word of given names).
    QString firstName() const;

    /// Returns the full name string (same as QString conversion).
    QString full() const { return *this; }

    /// Case-insensitive full name comparison.
    bool matches(const Name& other) const;

    /// Component-based matching: surname + first name (ignores middle names).
    /// Example: "Smith, John" matches "Smith, John Michael"
    bool matchesComponents(const Name& other) const;

    /// Check if a first name matches the first name portion of this name.
    bool firstNameMatches(const QString& firstName) const;

    // ========================================================================
    // Static classification/matching functions (for raw QString parsing)
    // ========================================================================

    /// Returns true if text is a surname only (letters/spaces, no comma).
    static bool isSurnameOnly(const QString& text);

    /// Returns true if text matches "Last, First" or "Last, First Middle" name format.
    static bool looksLikeFullName(const QString& text);

    /// Simple case-insensitive name comparison.
    static bool match(const QString& name1, const QString& name2);

    /// Component-based name matching (surname + first name separately).
    static bool matchComponents(const QString& name1, const QString& name2);

    /// Check if a first name matches the given name portion of a full name.
    static bool firstNameMatches(const QString& fullName, const QString& firstName);
};
