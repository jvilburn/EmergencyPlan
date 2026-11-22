#include "UpdateCheck.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

QString UpdateCheck::platform()
{
    return QSysInfo::productType();
}

std::optional<Release> UpdateCheck::later(const QByteArray& list, const QString& platform, int runningNumber)
{
    QJsonObject json = QJsonDocument::fromJson(list).object().value(platform).toObject();
    Release release{json.value("version").toString(), json.value("number").toInt(),
                    QUrl(json.value("url").toString())};
    if (release.number <= runningNumber || release.version.isEmpty() || !release.installer.isValid()
        || release.installer.isEmpty())
    {
        return std::nullopt;
    }
    return release;
}
