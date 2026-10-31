#include "Coordinates.h"

Coordinates::Coordinates(double latitude, double longitude)
    : m_latitude(latitude)
    , m_longitude(longitude)
{
}

QJsonObject Coordinates::toJson() const
{
    QJsonObject json;
    json["latitude"] = m_latitude;
    json["longitude"] = m_longitude;
    return json;
}

Coordinates Coordinates::fromJson(const QJsonObject& json)
{
    return Coordinates(json["latitude"].toDouble(), json["longitude"].toDouble());
}

bool Coordinates::operator==(const Coordinates& other) const
{
    return m_latitude == other.m_latitude
        && m_longitude == other.m_longitude;
}
