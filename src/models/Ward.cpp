#include "Ward.h"
#include "StrictJson.h"

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

std::optional<Ward> Ward::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("name"),
                                  QLatin1String("unitNumber"),
                                  QLatin1String("stakeUnitNumber"),
                                  QLatin1String("chapelAddress"),
                                  QLatin1String("chapelLat"),
                                  QLatin1String("chapelLng"),
                                  QLatin1String("chapelPhone"),
                                  QLatin1String("meetingTime")}))
    {
        return std::nullopt;
    }

    Ward ward;

    // The name and unit numbers are written only when not empty; the chapel fields
    // whenever set, empty or not.
    if (!StrictJson::readOptionalNonEmptyString(json, QLatin1String("name"), ward.m_name)
        || !StrictJson::readOptionalNonEmptyString(json, QLatin1String("unitNumber"), ward.m_unitNumber)
        || !StrictJson::readOptionalNonEmptyString(json, QLatin1String("stakeUnitNumber"), ward.m_stakeUnitNumber)
        || !StrictJson::readOptionalString(json, QLatin1String("chapelAddress"), ward.m_chapelAddress)
        || !StrictJson::readOptionalString(json, QLatin1String("chapelPhone"), ward.m_chapelPhone)
        || !StrictJson::readOptionalString(json, QLatin1String("meetingTime"), ward.m_meetingTime)
        || !StrictJson::readOptionalDouble(json, QLatin1String("chapelLat"), ward.m_chapelLat)
        || !StrictJson::readOptionalDouble(json, QLatin1String("chapelLng"), ward.m_chapelLng))
    {
        return std::nullopt;
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
