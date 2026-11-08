#include "Coordinates.h"

#include <QJsonArray>
#include <algorithm>

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

Coordinates Coordinates::fromJson(JsonReader json)
{
    double latitude = json.number("latitude");
    double longitude = json.number("longitude");
    json.finish();
    return Coordinates(latitude, longitude);
}

bool Coordinates::operator==(const Coordinates& other) const
{
    return m_latitude == other.m_latitude
        && m_longitude == other.m_longitude;
}

void writeAddressCoordinates(QJsonObject& json, const QString& key, const QHash<QString, Coordinates>& table)
{
    if (table.isEmpty())
    {
        return;
    }
    QStringList addresses = table.keys();
    std::sort(addresses.begin(), addresses.end());
    QJsonArray array;
    for (const QString& address : addresses)
    {
        QJsonObject entry = table.constFind(address)->toJson();
        entry["address"] = address;
        array.append(entry);
    }
    json[key] = array;
}

QHash<QString, Coordinates> readAddressCoordinates(JsonReader& json, const QString& key)
{
    QHash<QString, Coordinates> table;
    const QList<JsonReader> entries = json.optionalObjects(key);
    for (JsonReader entry : entries)
    {
        QString address = entry.string("address");
        table.insert(address, Coordinates::fromJson(entry));
    }
    return table;
}
