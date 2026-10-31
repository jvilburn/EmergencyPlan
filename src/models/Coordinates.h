#pragma once

#include <QJsonObject>

/// A place on the map.
class Coordinates
{
public:
    Coordinates(double latitude, double longitude);

    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }

    QJsonObject toJson() const;
    static Coordinates fromJson(const QJsonObject& json);

    bool operator==(const Coordinates& other) const;
    bool operator!=(const Coordinates& other) const { return !(*this == other); }

private:
    double m_latitude;
    double m_longitude;
};
