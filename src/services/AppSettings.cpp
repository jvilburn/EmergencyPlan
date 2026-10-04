#include "AppSettings.h"

#include "StrictJson.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QObject>
#include <QSaveFile>

namespace
{

// The address of the Ward Planning server this build talks to.
const QString DefaultServerAddress;

// Part of the settings format.
const QLatin1String KeyRecords("records");
const QLatin1String KeyDeveloperExport("developerExport");
const QLatin1String KeyServerAddress("serverAddress");
const QLatin1String KeyMap("map");
const QLatin1String KeyDefaultLat("defaultLat");
const QLatin1String KeyDefaultLng("defaultLng");

} // namespace

std::unique_ptr<AppSettings> AppSettings::open(const QString& path, QString* errorMessage)
{
    std::unique_ptr<AppSettings> settings(new AppSettings(path));

    QFile file(path);
    if (!file.exists())
    {
        return settings;
    }

    QString problem;
    if (!file.open(QIODevice::ReadOnly))
    {
        problem = QObject::tr("The settings file %1 could not be opened: %2.").arg(path, file.errorString());
    }
    else if (!settings->read(file.readAll()))
    {
        problem = QObject::tr("The settings file %1 is not in a form this version of Ward Planning reads.").arg(path);
    }

    if (problem.isEmpty())
    {
        return settings;
    }

    if (errorMessage != nullptr)
    {
        *errorMessage = problem + QLatin1Char(' ')
            + QObject::tr("Moving the file somewhere else starts Ward Planning with fresh settings.");
    }
    return nullptr;
}

AppSettings::AppSettings(const QString& path)
    : m_path(path)
{
}

// --- DocumentRecords -------------------------------------------------------------

DocumentRecord AppSettings::record(const QString& documentId) const
{
    return m_records.value(documentId);
}

void AppSettings::setRecord(const QString& documentId, const DocumentRecord& record)
{
    m_records.insert(documentId, record);
    save();
}

void AppSettings::removeRecord(const QString& documentId)
{
    m_records.remove(documentId);
    save();
}

QStringList AppSettings::documentIds() const
{
    return m_records.keys();
}

// --- This device -----------------------------------------------------------------

void AppSettings::setDeveloperExport(bool enabled)
{
    m_developerExport = enabled;
    save();
}

QString AppSettings::serverAddress() const
{
    return m_serverAddressOverride.isEmpty() ? DefaultServerAddress : m_serverAddressOverride;
}

void AppSettings::setServerAddressOverride(const QString& address)
{
    m_serverAddressOverride = address;
    save();
}

void AppSettings::setDefaultLocation(const Location& location)
{
    m_defaultLocation = location;
    save();
}

// --- The file --------------------------------------------------------------------

bool AppSettings::read(const QByteArray& bytes)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return false;
    }

    const QJsonObject json = document.object();
    QJsonObject records;
    if (!StrictJson::hasOnlyKeys(json, {KeyRecords, KeyDeveloperExport, KeyServerAddress, KeyMap})
        || !StrictJson::readObject(json, KeyRecords, records)
        || !StrictJson::readOptionalNonEmptyString(json, KeyServerAddress, m_serverAddressOverride))
    {
        return false;
    }

    // Written only when set, so present means true.
    if (json.contains(KeyDeveloperExport))
    {
        if (!StrictJson::readBool(json, KeyDeveloperExport, m_developerExport) || !m_developerExport)
        {
            return false;
        }
    }

    if (json.contains(KeyMap))
    {
        QJsonObject map;
        Location location;
        if (!StrictJson::readObject(json, KeyMap, map)
            || !StrictJson::hasOnlyKeys(map, {KeyDefaultLat, KeyDefaultLng})
            || !StrictJson::readDouble(map, KeyDefaultLat, location.latitude)
            || !StrictJson::readDouble(map, KeyDefaultLng, location.longitude))
        {
            return false;
        }
        m_defaultLocation = location;
    }

    for (auto entry = records.constBegin(); entry != records.constEnd(); ++entry)
    {
        if (entry.key().isEmpty() || !entry.value().isObject())
        {
            return false;
        }

        const std::optional<DocumentRecord> record = DocumentRecord::fromJson(entry.value().toObject());
        if (!record.has_value())
        {
            return false;
        }
        m_records.insert(entry.key(), record.value());
    }

    return true;
}

QByteArray AppSettings::write() const
{
    QJsonObject records;
    for (auto entry = m_records.constBegin(); entry != m_records.constEnd(); ++entry)
    {
        records[entry.key()] = entry.value().toJson();
    }

    QJsonObject json;
    json[KeyRecords] = records;
    if (m_developerExport)
    {
        json[KeyDeveloperExport] = true;
    }
    if (!m_serverAddressOverride.isEmpty())
    {
        json[KeyServerAddress] = m_serverAddressOverride;
    }
    if (m_defaultLocation.has_value())
    {
        QJsonObject map;
        map[KeyDefaultLat] = m_defaultLocation.value().latitude;
        map[KeyDefaultLng] = m_defaultLocation.value().longitude;
        json[KeyMap] = map;
    }

    return QJsonDocument(json).toJson(QJsonDocument::Indented);
}

void AppSettings::save()
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());

    QSaveFile file(m_path);
    const QByteArray bytes = write();
    m_writeFailed = !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit();
    if (m_writeFailed)
    {
        qWarning() << "Settings could not be written to" << m_path << ":" << file.errorString();
    }
}
