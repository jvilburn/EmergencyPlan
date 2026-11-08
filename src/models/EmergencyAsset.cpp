#include "EmergencyAsset.h"

#include <QJsonArray>

EmergencyAsset EmergencyAsset::create(const QString& name, ResponseArea area)
{
    EmergencyAsset asset;
    asset.m_id = EmergencyAssetId::generate();
    asset.m_name = name;
    asset.m_responseArea = area;
    return asset;
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

    json["personIds"] = sortedIdArray(m_personIds);

    return json;
}

EmergencyAsset EmergencyAsset::fromJson(JsonReader json)
{
    EmergencyAsset asset;
    asset.m_id = json.id<EmergencyAssetId>("id");
    asset.m_name = json.string("name");
    asset.m_responseArea = json.word("responseArea", assetAreas(), &responseAreaToString);
    asset.m_personIds = json.ids<PersonId>("personIds");
    json.finish();
    return asset;
}

bool EmergencyAsset::operator==(const EmergencyAsset& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_responseArea == other.m_responseArea
        && m_personIds == other.m_personIds;
}
