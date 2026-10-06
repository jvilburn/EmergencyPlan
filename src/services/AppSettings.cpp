#include "AppSettings.h"

#include "StrictJson.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QObject>
#include <QSaveFile>

namespace
{

// The address of the Ward Planning server this build talks to.
const QString DefaultServerAddress;

// Part of the settings format.
const QLatin1String KeyRecords("records");
const QLatin1String KeyWards("wards");
const QLatin1String KeyWardName("wardName");
const QLatin1String KeyLastOpened("lastOpened");
const QLatin1String KeyCopyPaths("copyPaths");
const QLatin1String KeyLastWard("lastWard");
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

// --- The document library's wards -------------------------------------------------

bool AppSettings::WardEntry::operator==(const WardEntry& other) const
{
    return wardName == other.wardName
        && lastOpened == other.lastOpened
        && copyPaths == other.copyPaths;
}

std::optional<AppSettings::WardEntry> AppSettings::ward(const QString& documentId) const
{
    const auto found = m_wards.constFind(documentId);
    if (found == m_wards.constEnd())
    {
        return std::nullopt;
    }
    return found.value();
}

void AppSettings::setWard(const QString& documentId, const WardEntry& entry)
{
    m_wards.insert(documentId, entry);
    save();
}

void AppSettings::removeWard(const QString& documentId)
{
    m_wards.remove(documentId);
    if (m_lastWardId == documentId)
    {
        m_lastWardId.clear();
    }
    save();
}

void AppSettings::setLastWardId(const QString& documentId)
{
    m_lastWardId = documentId;
    save();
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
    QJsonObject wards;
    if (!StrictJson::hasOnlyKeys(json, {KeyRecords, KeyWards, KeyLastWard, KeyDeveloperExport, KeyServerAddress, KeyMap})
        || !StrictJson::readObject(json, KeyRecords, records)
        || !StrictJson::readObject(json, KeyWards, wards)
        || !StrictJson::readOptionalNonEmptyString(json, KeyLastWard, m_lastWardId)
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

    for (auto entry = wards.constBegin(); entry != wards.constEnd(); ++entry)
    {
        if (entry.key().isEmpty() || !entry.value().isObject())
        {
            return false;
        }

        const QJsonObject wardJson = entry.value().toObject();
        WardEntry ward;
        if (!StrictJson::hasOnlyKeys(wardJson, {KeyWardName, KeyLastOpened, KeyCopyPaths})
            || !StrictJson::readNonEmptyString(wardJson, KeyWardName, ward.wardName))
        {
            return false;
        }

        if (wardJson.contains(KeyLastOpened))
        {
            QDateTime lastOpened;
            if (!StrictJson::readTime(wardJson, KeyLastOpened, lastOpened))
            {
                return false;
            }
            ward.lastOpened = lastOpened;
        }

        // Written only when there are some, so present means not empty.
        if (wardJson.contains(KeyCopyPaths)
            && (!StrictJson::readStringArray(wardJson, KeyCopyPaths, ward.copyPaths) || ward.copyPaths.isEmpty()))
        {
            return false;
        }

        m_wards.insert(entry.key(), ward);
    }

    // The last ward is one in the library.
    if (!m_lastWardId.isEmpty() && !m_wards.contains(m_lastWardId))
    {
        return false;
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

    QJsonObject wards;
    for (auto entry = m_wards.constBegin(); entry != m_wards.constEnd(); ++entry)
    {
        QJsonObject wardJson;
        wardJson[KeyWardName] = entry.value().wardName;
        if (entry.value().lastOpened.has_value())
        {
            wardJson[KeyLastOpened] = StrictJson::writeTime(entry.value().lastOpened.value());
        }
        if (!entry.value().copyPaths.isEmpty())
        {
            wardJson[KeyCopyPaths] = QJsonArray::fromStringList(entry.value().copyPaths);
        }
        wards[entry.key()] = wardJson;
    }

    QJsonObject json;
    json[KeyRecords] = records;
    json[KeyWards] = wards;
    if (!m_lastWardId.isEmpty())
    {
        json[KeyLastWard] = m_lastWardId;
    }
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
