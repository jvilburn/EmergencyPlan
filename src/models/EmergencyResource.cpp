#include "EmergencyResource.h"

#include <QJsonArray>

EmergencyResource EmergencyResource::create(const QString& name, ResponseArea area)
{
    EmergencyResource resource;
    resource.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    resource.m_name = name;
    resource.m_responseArea = area;
    return resource;
}

void EmergencyResource::addPerson(const QString& personId)
{
    m_personIds.insert(personId);
}

void EmergencyResource::removePerson(const QString& personId)
{
    m_personIds.remove(personId);
}

bool EmergencyResource::hasPerson(const QString& personId) const
{
    return m_personIds.contains(personId);
}

QJsonObject EmergencyResource::toJson() const
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

EmergencyResource EmergencyResource::fromJson(const QJsonObject& json)
{
    EmergencyResource resource;
    resource.m_id = json["id"].toString();
    resource.m_name = json["name"].toString();
    resource.m_responseArea = responseAreaFromString(json["responseArea"].toString());

    const QJsonArray personArray = json["personIds"].toArray();
    for (const QJsonValue& val : personArray)
    {
        resource.m_personIds.insert(val.toString());
    }

    return resource;
}

bool EmergencyResource::operator==(const EmergencyResource& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_responseArea == other.m_responseArea
        && m_personIds == other.m_personIds;
}
