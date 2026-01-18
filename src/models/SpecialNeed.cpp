#include "SpecialNeed.h"

QJsonObject SpecialNeed::toJson() const
{
    QJsonObject json;
    if (personId.has_value())
    {
        json["personId"] = personId.value();
    }
    if (familyId.has_value())
    {
        json["familyId"] = familyId.value();
    }
    json["note"] = note;
    return json;
}

SpecialNeed SpecialNeed::fromJson(const QJsonObject& json)
{
    SpecialNeed need;
    if (json.contains("personId") && !json["personId"].isNull())
    {
        need.personId = json["personId"].toString();
    }
    if (json.contains("familyId") && !json["familyId"].isNull())
    {
        need.familyId = json["familyId"].toString();
    }
    need.note = json["note"].toString();
    return need;
}

bool SpecialNeed::operator==(const SpecialNeed& other) const
{
    return personId == other.personId
        && familyId == other.familyId
        && note == other.note;
}

bool SpecialNeed::matchesEntity(const std::optional<QString>& pId, const std::optional<QString>& fId) const
{
    return personId == pId && familyId == fId;
}
