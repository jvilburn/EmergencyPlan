#include "AppSettings.h"

#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

AppSettings::AppSettings(const QString& path)
    : m_path(path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly))
    {
        m_json = QJsonDocument::fromJson(file.readAll()).object();
    }
}

void AppSettings::setUseServer(bool use)
{
    m_json["useServer"] = use;
    save();
}

void AppSettings::setDeviceName(const QString& name)
{
    m_json["deviceName"] = name;
    save();
}

void AppSettings::setUserName(const QString& name)
{
    m_json["userName"] = name;
    save();
}

void AppSettings::setEmail(const QString& email)
{
    m_json["email"] = email;
    save();
}

std::optional<DocumentId> AppSettings::lastWard() const
{
    if (!m_json.contains("lastWardId"))
    {
        return std::nullopt;
    }
    return DocumentId::fromString(m_json.value("lastWardId").toString());
}

void AppSettings::setLastWard(const DocumentId& document)
{
    m_json["lastWardId"] = document.toString();
    save();
}

std::optional<Coordinates> AppSettings::mapHome() const
{
    if (!m_json.contains("defaultLat") || !m_json.contains("defaultLng"))
    {
        return std::nullopt;
    }
    return Coordinates(m_json.value("defaultLat").toDouble(), m_json.value("defaultLng").toDouble());
}

void AppSettings::setMapHome(const Coordinates& home)
{
    m_json["defaultLat"] = home.latitude();
    m_json["defaultLng"] = home.longitude();
    save();
}

void AppSettings::save() const
{
    QSaveFile file(m_path);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(m_json).toJson());
        file.commit();
    }
}
