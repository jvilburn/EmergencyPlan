#pragma once

#include <QString>
#include <QJsonObject>
#include <optional>

struct SpecialNeed
{
    std::optional<QString> personId;
    std::optional<QString> familyId;
    QString note;

    // JSON serialization
    QJsonObject toJson() const;
    static SpecialNeed fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const SpecialNeed& other) const;
    bool operator!=(const SpecialNeed& other) const { return !(*this == other); }

    // Identity check (same person or family)
    bool matchesEntity(const std::optional<QString>& pId, const std::optional<QString>& fId) const;
};
