#pragma once

#include <QString>
#include <QStringList>

/// Type-safe wrapper for addresses stored as list of lines.
/// Uses composition to ensure all lines are sanitized.
class Address
{
public:
    /// Default constructor - creates empty address.
    Address() = default;

    /// Add a single line to the address (sanitizes the input).
    void addLine(const QString& line);

    /// Returns full address as single line, comma-separated.
    QString full() const;

    /// Returns address with lines separated by newlines (for display).
    QString multiLine() const;

    /// Returns true if address has no lines.
    bool isEmpty() const;

    /// Returns true if address has no street (only city/state/zip - not useful for geocoding).
    bool noStreetAddress() const;

    /// Returns number of lines.
    int size() const;

    /// Returns the lines (for iteration).
    const QStringList& lines() const;

    /// Equality comparison (compares lines exactly).
    bool operator==(const Address& other) const;
    bool operator!=(const Address& other) const;

private:
    /// Sanitize a single line: remove control characters, trim whitespace.
    static QString sanitizeLine(const QString& line);

    QStringList m_lines;
};
