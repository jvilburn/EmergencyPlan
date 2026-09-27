#include "EmergencyAsset.h"
#include "StrictJson.h"

#include <QJsonArray>

EmergencyAsset EmergencyAsset::create(const QString& name, ResponseArea area)
{
    EmergencyAsset asset;
    asset.m_id = EmergencyAssetId::generate();
    asset.m_name = name;
    asset.m_responseArea = area;
    return asset;
}

void EmergencyAsset::addPerson(const PersonId& personId)
{
    m_personIds.insert(personId);
}

void EmergencyAsset::removePerson(const PersonId& personId)
{
    m_personIds.remove(personId);
}

bool EmergencyAsset::hasPerson(const PersonId& personId) const
{
    return m_personIds.contains(personId);
}

QJsonObject EmergencyAsset::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["name"] = m_name;
    json["responseArea"] = responseAreaToString(m_responseArea);

    QJsonArray personArray;
    for (const PersonId& personId : m_personIds)
    {
        personArray.append(personId.toString());
    }
    json["personIds"] = personArray;

    return json;
}

std::optional<EmergencyAsset> EmergencyAsset::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("id"),
                                  QLatin1String("name"),
                                  QLatin1String("responseArea"),
                                  QLatin1String("personIds")}))
    {
        return std::nullopt;
    }

    EmergencyAsset asset;
    QString area;
    if (!StrictJson::readId(json, QLatin1String("id"), asset.m_id)
        || !StrictJson::readString(json, QLatin1String("name"), asset.m_name)
        || !StrictJson::readString(json, QLatin1String("responseArea"), area)
        || !StrictJson::readIdSet(json, QLatin1String("personIds"), asset.m_personIds))
    {
        return std::nullopt;
    }

    // Exactly as responseAreaToString writes it: an asset's own area, or none.
    bool known = false;
    for (const ResponseArea candidate :
         {ResponseArea::Medical, ResponseArea::Recovery, ResponseArea::Communications, ResponseArea::None})
    {
        if (area == responseAreaToString(candidate))
        {
            asset.m_responseArea = candidate;
            known = true;
            break;
        }
    }
    if (!known)
    {
        return std::nullopt;
    }

    return asset;
}

bool EmergencyAsset::operator==(const EmergencyAsset& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_responseArea == other.m_responseArea
        && m_personIds == other.m_personIds;
}
