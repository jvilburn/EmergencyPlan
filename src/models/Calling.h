#pragma once

#include <QString>

/// Type-safe wrapper for calling names stored as QString.
/// Inherits from QString for seamless interoperability.
struct Calling : public QString
{
public:
    using QString::QString;

    /// Construct from QString (enables implicit conversion).
    Calling(const QString& str) : QString(str) {}

    // ========================================================================
    // Static classification function (for PDF parsing)
    // ========================================================================

    /// Returns true if text is letters and spaces only (basic calling text heuristic).
    static bool isCalling(const QString& text);
};
