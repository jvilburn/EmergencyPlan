// geolocation.cpp
#include "geolocation.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

GeoLocation::GeoLocation(QObject *parent)
    : QObject(parent)
    , manager(new QNetworkAccessManager(this))
{
}

void GeoLocation::requestLocation() {
    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl("https://ipinfo.io/json")));
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            emit locationError(reply->errorString());
            return;
        }
        
        QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        QStringList loc = json["loc"].toString().split(',');
        
        if (loc.size() == 2) {
            emit locationReady(loc[0].toDouble(), loc[1].toDouble());
        } else {
            emit locationError("Invalid response format");
        }
    });
}