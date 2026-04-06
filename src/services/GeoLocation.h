#pragma once

#include <QObject>
#include <QNetworkAccessManager>

class GeoLocation : public QObject {
    Q_OBJECT
public:
    explicit GeoLocation(QObject *parent);
    void requestLocation();

signals:
    void locationReady(double latitude, double longitude);
    void locationError(const QString &error);

private:
    QNetworkAccessManager *manager;
};
