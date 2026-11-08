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

    if (!m_wardUnitNumbers.isEmpty())
    {
        QStringList wardUnits = m_wardUnitNumbers.values();
        wardUnits.sort();
        json["wardUnitNumbers"] = QJsonArray::fromStringList(wardUnits);
    }

    return json;
}

Stake Stake::fromJson(JsonReader json)
{
    Stake stake;
    stake.m_name = json.optionalString("name");
    stake.m_unitNumber = json.optionalString("unitNumber");
    stake.m_stakeCenterAddress = json.optionalString("stakeCenterAddress");
    stake.m_stakeCenterLat = json.optionalNumber("stakeCenterLat");
    stake.m_stakeCenterLng = json.optionalNumber("stakeCenterLng");
    const QStringList wardUnits = json.optionalStrings("wardUnitNumbers");
    stake.m_wardUnitNumbers = QSet<QString>(wardUnits.begin(), wardUnits.end());
    json.finish();
    return stake;
}

// ============================================================================
// Equality
// ============================================================================

bool Stake::operator==(const Stake& other) const
{
    return m_name == other.m_name
        && m_unitNumber == other.m_unitNumber
        && m_stakeCenterAddress == other.m_stakeCenterAddress
        && m_stakeCenterLat == other.m_stakeCenterLat
        && m_stakeCenterLng == other.m_stakeCenterLng
        && m_wardUnitNumbers == other.m_wardUnitNumbers;
}
