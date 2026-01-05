#pragma once

#include <QString>

/// Type-safe wrapper for email addresses stored as QString.
/// Inherits from QString for seamless interoperability.
struct Email : public QString
{
public:
    using QString::QString;

    /// Construct from QString (enables implicit conversion).
    Email(const QString& str) : QString(str) {}

    // ========================================================================
    // Static classification function (for PDF parsing)
    // ========================================================================

    /// Returns true if text contains '@'.
    static bool isEmail(const QString& text);
};
