#include "Ward.h"

void Ward::setName(const QString& name)
{
    if (name.endsWith(" Ward"))
    {
        m_name = name.left(name.length() - 5);
    }
    else
    {
        m_name = name;
    }
}

QJsonObject Ward::toJson() const
{
    QJsonObject json;

    if (!m_name.isEmpty())
    {
        json["name"] = m_name;
    }
    if (!m_unitNumber.isEmpty())
    {
        json["unitNumber"] = m_unitNumber;
    }
    if (!m_stakeUnitNumber.isEmpty())
    {
        json["stakeUnitNumber"] = m_stakeUnitNumber;
    }
    if (m_chapelAddress)
    {
        json["chapelAddress"] = *m_chapelAddress;
    }
    if (m_chapelLat)
    {
        json["chapelLat"] = *m_chapelLat;
    }
    if (m_chapelLng)
    {
        json["chapelLng"] = *m_chapelLng;
    }
    if (m_chapelPhone)
    {
        json["chapelPhone"] = *m_chapelPhone;
    }
    if (m_meetingTime)
    {
        json["meetingTime"] = *m_meetingTime;
    }

    return json;
}

Ward Ward::fromJson(const QJsonObject& json)
{
    Ward ward;

    ward.m_name = json["name"].toString();
    ward.m_unitNumber = json["unitNumber"].toString();
    ward.m_stakeUnitNumber = json["stakeUnitNumber"].toString();

    if (json.contains("chapelAddress"))
    {
        ward.m_chapelAddress = json["chapelAddress"].toString();
    }
    if (json.contains("chapelLat"))
    {
        ward.m_chapelLat = json["chapelLat"].toDouble();
    }
    if (json.contains("chapelLng"))
    {
        ward.m_chapelLng = json["chapelLng"].toDouble();
    }
    if (json.contains("chapelPhone"))
    {
        ward.m_chapelPhone = json["chapelPhone"].toString();
    }
    if (json.contains("meetingTime"))
    {
        ward.m_meetingTime = json["meetingTime"].toString();
    }

    return ward;
}

bool Ward::operator==(const Ward& other) const
{
    return m_name == other.m_name
        && m_unitNumber == other.m_unitNumber
        && m_stakeUnitNumber == other.m_stakeUnitNumber
        && m_chapelAddress == other.m_chapelAddress
        && m_chapelLat == other.m_chapelLat
        && m_chapelLng == other.m_chapelLng
        && m_chapelPhone == other.m_chapelPhone
        && m_meetingTime == other.m_meetingTime;
}
