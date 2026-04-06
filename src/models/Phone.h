#pragma once

#include <QString>

/// Type-safe wrapper for phone numbers stored as QString.
/// Inherits from QString for seamless interoperability.
struct Phone : public QString
{
public:
    using QString::QString;

    /// Construct from QString (enables implicit conversion).
    Phone(const QString& str) : QString(str) {}

    /// Extract digits only from this phone number.
    QString digits() const;

    /// Check if this phone number has at least 7 digits (valid).
    bool isValid() const;

    /// Digit-based comparison with another phone number.
    bool matches(const Phone& other) const;

    // ========================================================================
    // Static classification/matching functions (for raw QString parsing)
    // ========================================================================

    /// Returns true if text looks like a phone number (starts with digit/(/+, has 7+ digits).
    static bool isPhone(const QString& text);

    /// Check if two phone numbers match (digits only comparison).
    static bool match(const QString& phone1, const QString& phone2);

    /// Extract digits from a phone number string.
    static QString extractDigits(const QString& phone);
};
