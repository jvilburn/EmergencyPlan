#pragma once

#include "Coordinates.h"

#include <QObject>
#include <QString>

/// Turns an address's text into its coordinates. Each request answers once, with resolved
/// or failed.
class Geocoder : public QObject
{
    Q_OBJECT

public:
    virtual void geocode(const QString& address) = 0;

signals:
    void resolved(const QString& address, const Coordinates& coordinates);
    void failed(const QString& address);

protected:
    explicit Geocoder(QObject* parent);
};
