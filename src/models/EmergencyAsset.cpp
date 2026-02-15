#include "EmergencyAsset.h"

#include <QJsonArray>

EmergencyAsset EmergencyAsset::create(const QString& name, ResponseArea area)
{
    EmergencyAsset asset;
    asset.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    asset.m_name = name;
    asset.m_responseArea = area;
    return asset;
}

void EmergencyAsset::addPerson(const QString& personId)
{
    m_personIds.insert(personId);
}

void EmergencyAsset::removePerson(const QString& personId)
{
    m_personIds.remove(personId);
}

bool EmergencyAsset::hasPerson(const QString& personId) const
{
    return m_personIds.contains(personId);
}

QJsonObject EmergencyAsset::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["responseArea"] = responseAreaToString(m_responseArea);

    QJsonArray personArray;
    for (const QString& personId : m_personIds)
    {
        personArray.append(personId);
    }
    json["personIds"] = personArray;

    return json;
}

EmergencyAsset EmergencyAsset::fromJson(const QJsonObject& json)
{
    EmergencyAsset asset;
    asset.m_id = json["id"].toString();
    asset.m_name = json["name"].toString();
    asset.m_responseArea = responseAreaFromString(json["responseArea"].toString());

    const QJsonArray personArray = json["personIds"].toArray();
    for (const QJsonValue& val : personArray)
    {
        asset.m_personIds.insert(val.toString());
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
