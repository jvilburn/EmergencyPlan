#include "Stake.h"
#include "StrictJson.h"

void Stake::setName(const QString& name)
{
    if (name.endsWith(" Stake"))
    {
        m_name = name.left(name.length() - 6);
    }
    else
    {
        m_name = name;
    }
}

// ============================================================================
// JSON serialization
// ============================================================================

QJsonObject Stake::toJson() const
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
    if (!m_phone.isEmpty())
    {
        json["phone"] = m_phone;
    }
    if (!m_email.isEmpty())
    {
        json["email"] = m_email;
    }
    if (!m_stakeCenterAddress.isEmpty())
    {
        json["stakeCenterAddress"] = m_stakeCenterAddress;
    }
    if (m_stakeCenterLat)
    {
        json["stakeCenterLat"] = *m_stakeCenterLat;
    }
    if (m_stakeCenterLng)
    {
        json["stakeCenterLng"] = *m_stakeCenterLng;
    }

    // Serialize ward unit numbers
    if (!m_wardUnitNumbers.isEmpty())
    {
        QJsonArray wardUnitsArray;
        for (const QString& wardUnit : m_wardUnitNumbers)
        {
            wardUnitsArray.append(wardUnit);
        }
        json["wardUnitNumbers"] = wardUnitsArray;
    }

    return json;
}

std::optional<Stake> Stake::fromJson(const QJsonObject& json)
{
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("name"),
                                  QLatin1String("unitNumber"),
                                  QLatin1String("phone"),
                                  QLatin1String("email"),
                                  QLatin1String("stakeCenterAddress"),
                                  QLatin1String("stakeCenterLat"),
                                  QLatin1String("stakeCenterLng"),
                                  QLatin1String("wardUnitNumbers")}))
    {
        return std::nullopt;
    }

    Stake stake;

    // Written only when not empty.
    for (const auto& [key, field] : {std::pair{QLatin1String("name"), &stake.m_name},
                                     std::pair{QLatin1String("unitNumber"), &stake.m_unitNumber},
                                     std::pair{QLatin1String("phone"), &stake.m_phone},
                                     std::pair{QLatin1String("email"), &stake.m_email},
                                     std::pair{QLatin1String("stakeCenterAddress"), &stake.m_stakeCenterAddress}})
    {
        if (json.contains(key) && !StrictJson::readNonEmptyString(json, key, *field))
        {
            return std::nullopt;
        }
    }

    double coordinate = 0.0;
    for (const auto& [key, field] : {std::pair{QLatin1String("stakeCenterLat"), &stake.m_stakeCenterLat},
                                     std::pair{QLatin1String("stakeCenterLng"), &stake.m_stakeCenterLng}})
    {
        if (json.contains(key))
        {
            if (!StrictJson::readDouble(json, key, coordinate))
            {
                return std::nullopt;
            }
            *field = coordinate;
        }
    }

    if (json.contains("wardUnitNumbers"))
    {
        QStringList units;
        if (!StrictJson::readStringArray(json, QLatin1String("wardUnitNumbers"), units) || units.isEmpty())
        {
            return std::nullopt;
        }
        for (const QString& unit : units)
        {
            if (unit.isEmpty() || stake.m_wardUnitNumbers.contains(unit))
            {
                return std::nullopt;
            }
            stake.m_wardUnitNumbers.insert(unit);
        }
    }

    return stake;
}

// ============================================================================
// Equality
// ============================================================================

bool Stake::operator==(const Stake& other) const
{
    return m_name == other.m_name
        && m_unitNumber == other.m_unitNumber
        && m_phone == other.m_phone
        && m_email == other.m_email
        && m_stakeCenterAddress == other.m_stakeCenterAddress
        && m_stakeCenterLat == other.m_stakeCenterLat
        && m_stakeCenterLng == other.m_stakeCenterLng
        && m_wardUnitNumbers == other.m_wardUnitNumbers;
}
