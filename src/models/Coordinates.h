#pragma once

#include "JsonReader.h"

#include <QHash>
#include <QJsonObject>
#include <QString>

/// A place on the map.
class Coordinates
{
public:
    Coordinates(double latitude, double longitude);

    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }

    QJsonObject toJson() const;
    static Coordinates fromJson(JsonReader json);

    bool operator==(const Coordinates& other) const;
    bool operator!=(const Coordinates& other) const { return !(*this == other); }

private:
    double m_latitude;
    double m_longitude;
};

/// Writes a table of address coordinates under key as an array of {address, latitude,
/// longitude} in address order, or nothing when it is empty.
void writeAddressCoordinates(QJsonObject& json, const QString& key, const QHash<QString, Coordinates>& table);

/// The table of address coordinates under key, which its writer leaves out when empty.
QHash<QString, Coordinates> readAddressCoordinates(JsonReader& json, const QString& key);
