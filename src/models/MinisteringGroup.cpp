#include "MinisteringGroup.h"

MinisteringGroup MinisteringGroup::createEQ(
    const QSet<PersonId>& ministerIds,
    const QSet<FamilyId>& familyIds)
{
    MinisteringGroup group;
    group.m_id = MinisteringGroupId::generate();
    group.m_isRSFormat = false;
    group.m_ministerIds = ministerIds;
    group.m_familyIds = familyIds;
    return group;
}

MinisteringGroup MinisteringGroup::createRS(
    const QSet<PersonId>& ministerIds,
    const QSet<PersonId>& ministeredPersonIds)
{
    MinisteringGroup group;
    group.m_id = MinisteringGroupId::generate();
    group.m_isRSFormat = true;
    group.m_ministerIds = ministerIds;
    group.m_ministeredPersonIds = ministeredPersonIds;
    return group;
}

QJsonObject MinisteringGroup::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["isRSFormat"] = m_isRSFormat;

    if (!m_ministerIds.isEmpty())
    {
        QJsonArray ministerIdsArray;
        for (const PersonId& id : m_ministerIds)
        {
            ministerIdsArray.append(id.toString());
        }
        json["ministerIds"] = ministerIdsArray;
    }

    if (!m_familyIds.isEmpty())
    {
        QJsonArray familyIdsArray;
        for (const FamilyId& id : m_familyIds)
        {
            familyIdsArray.append(id.toString());
        }
        json["familyIds"] = familyIdsArray;
    }

    if (!m_ministeredPersonIds.isEmpty())
    {
        QJsonArray ministeredPersonIdsArray;
        for (const PersonId& id : m_ministeredPersonIds)
        {
            ministeredPersonIdsArray.append(id.toString());
        }
        json["ministeredPersonIds"] = ministeredPersonIdsArray;
    }

    if (m_interviewedDate.has_value())
    {
        json["interviewedDate"] = m_interviewedDate->toString(Qt::ISODate);
    }

    if (m_presidencyMemberId.has_value())
    {
        json["presidencyMemberId"] = m_presidencyMemberId->toString();
    }

    return json;
}

MinisteringGroup MinisteringGroup::fromJson(const QJsonObject& json)
{
    MinisteringGroup group;
    group.m_id = MinisteringGroupId::fromString(json["id"].toString());
    group.m_isRSFormat = json["isRSFormat"].toBool();

    if (json.contains("ministerIds"))
    {
        QJsonArray ministerIdsArray = json["ministerIds"].toArray();
        for (const QJsonValue& value : ministerIdsArray)
        {
            group.m_ministerIds.insert(PersonId::fromString(value.toString()));
        }
    }

    if (json.contains("familyIds"))
    {
        QJsonArray familyIdsArray = json["familyIds"].toArray();
        for (const QJsonValue& value : familyIdsArray)
        {
            group.m_familyIds.insert(FamilyId::fromString(value.toString()));
        }
    }

    if (json.contains("ministeredPersonIds"))
    {
        QJsonArray ministeredPersonIdsArray = json["ministeredPersonIds"].toArray();
        for (const QJsonValue& value : ministeredPersonIdsArray)
        {
            group.m_ministeredPersonIds.insert(PersonId::fromString(value.toString()));
        }
    }

    if (json.contains("interviewedDate") && !json["interviewedDate"].isNull())
    {
        group.m_interviewedDate = QDate::fromString(json["interviewedDate"].toString(), Qt::ISODate);
    }

    if (json.contains("presidencyMemberId") && !json["presidencyMemberId"].isNull())
    {
        QString pmStr = json["presidencyMemberId"].toString();
        if (!pmStr.isEmpty())
        {
            group.m_presidencyMemberId = PersonId::fromString(pmStr);
        }
    }

    return group;
}

bool MinisteringGroup::operator==(const MinisteringGroup& other) const
{
    return m_id == other.m_id
        && m_isRSFormat == other.m_isRSFormat
        && m_ministerIds == other.m_ministerIds
        && m_familyIds == other.m_familyIds
        && m_ministeredPersonIds == other.m_ministeredPersonIds
        && m_interviewedDate == other.m_interviewedDate
        && m_presidencyMemberId == other.m_presidencyMemberId;
}
