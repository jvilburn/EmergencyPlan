#pragma once

#include <QString>
#include <QJsonObject>
#include <optional>

/// Encapsulates birth date with optional year, month, day.
/// Year may be null for adults (only month/day known from directory).
class Birthday
{
public:
    Birthday() = default;

    // Factory method for creating birthdays
    static Birthday create(std::optional<int> year,
                           std::optional<int> month,
                           std::optional<int> day);

    // Getters
    std::optional<int> year() const { return m_year; }
    std::optional<int> month() const { return m_month; }
    std::optional<int> day() const { return m_day; }

    // Computed properties
    bool hasDate() const { return m_month.has_value() && m_day.has_value(); }
    bool isEmpty() const { return !m_year.has_value() && !m_month.has_value() && !m_day.has_value(); }

    /// Calculate age from current date. Returns nullopt if year unknown.
    std::optional<int> age() const;

    /// Returns true if age < 18.
    bool isChild() const;

    /// Returns "19 Nov" or "19 Nov 2010" if year known.
    QString dateDisplay() const;

    /// The parts that are set, whichever they are: "Jan 4, 1980", "Jan 4", "Jan 1980",
    /// "1980", "Jan", "day 4, 1980", "day 4", or empty when none is set.
    QString display() const;

    /// Returns "(15)" for children, empty string for adults.
    QString ageDisplay() const;

    // JSON serialization
    QJsonObject toJson() const;
    static Birthday fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const Birthday& other) const;
    bool operator!=(const Birthday& other) const { return !(*this == other); }

    // ========================================================================
    // Static classification function (for PDF parsing)
    // ========================================================================

    /// Returns true if text matches birthday pattern like "7 Oct" or "23 May (16)".
    static bool isBirthday(const QString& text);

private:
    std::optional<int> m_year;   // null for adults
    std::optional<int> m_month;  // 1-12
    std::optional<int> m_day;    // 1-31
};
