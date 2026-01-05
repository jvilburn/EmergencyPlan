#pragma once

#include <QString>
#include <QJsonValue>
#include <optional>

/// Type-safe gender enum with JSON serialization.
class Gender
{
public:
    static const Gender Male;
    static const Gender Female;

    bool operator==(const Gender& other) const { return m_value == other.m_value; }
    bool operator!=(const Gender& other) const { return m_value != other.m_value; }

    QString toString() const;
    QJsonValue toJson() const;

    /// Returns true if text is "Male" or "Female".
    static bool isGender(const QString& text);
    static std::optional<Gender> fromString(const QString& str);
    static std::optional<Gender> fromJson(const QJsonValue& value);

private:
    int m_value;
    Gender(int v) : m_value(v) {}
};
