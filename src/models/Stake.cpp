#include "Stake.h"

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

Stake Stake::fromJson(const QJsonObject& json)
{
    Stake stake;

    stake.m_unitNumber = json["unitNumber"].toString();
    stake.m_name = json["name"].toString();
    stake.m_phone = json["phone"].toString();
    stake.m_email = json["email"].toString();
    stake.m_stakeCenterAddress = json["stakeCenterAddress"].toString();

    if (json.contains("stakeCenterLat"))
    {
        stake.m_stakeCenterLat = json["stakeCenterLat"].toDouble();
    }
    if (json.contains("stakeCenterLng"))
    {
        stake.m_stakeCenterLng = json["stakeCenterLng"].toDouble();
    }

    // Deserialize ward unit numbers
    if (json.contains("wardUnitNumbers"))
    {
        QJsonArray wardUnitsArray = json["wardUnitNumbers"].toArray();
        for (const QJsonValue& value : wardUnitsArray)
        {
            stake.m_wardUnitNumbers.insert(value.toString());
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
