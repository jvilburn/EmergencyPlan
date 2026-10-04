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
const QLatin1String KeyDocuments("documents");
const QLatin1String KeyRecord("record");
const QLatin1String KeyWardName("wardName");
const QLatin1String KeyLastOpened("lastOpened");
const QLatin1String KeyCopyPaths("copyPaths");
const QLatin1String KeyLastDocumentId("lastDocumentId");
const QLatin1String KeyDisplayName("displayName");
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

    if (!file.open(QIODevice::ReadOnly) || !settings->read(file.readAll()))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QObject::tr("The settings file %1 could not be read.").arg(path);
        }
        return nullptr;
    }

    return settings;
}

AppSettings::AppSettings(const QString& path)
    : m_path(path)
{
}

// --- DocumentRecords -------------------------------------------------------------

DocumentRecord AppSettings::record(const QString& documentId) const
{
    return m_documents.value(documentId).record;
}

void AppSettings::setRecord(const QString& documentId, const DocumentRecord& record)
{
    m_documents[documentId].record = record;
    save();
}

void AppSettings::removeRecord(const QString& documentId)
{
    m_documents.remove(documentId);
    if (m_lastDocumentId == documentId)
    {
        m_lastDocumentId.clear();
    }
    save();
}

QStringList AppSettings::documentIds() const
{
    return m_documents.keys();
}

// --- Library entries -------------------------------------------------------------

QString AppSettings::wardName(const QString& documentId) const
{
    return m_documents.value(documentId).wardName;
}

void AppSettings::setWardName(const QString& documentId, const QString& wardName)
{
    m_documents[documentId].wardName = wardName;
    save();
}

std::optional<QDateTime> AppSettings::lastOpened(const QString& documentId) const
{
    return m_documents.value(documentId).lastOpened;
}

void AppSettings::setLastOpened(const QString& documentId, const QDateTime& lastOpened)
{
    m_documents[documentId].lastOpened = lastOpened;
    save();
}

QStringList AppSettings::copyPaths(const QString& documentId) const
{
    return m_documents.value(documentId).copyPaths;
}

void AppSettings::setCopyPaths(const QString& documentId, const QStringList& paths)
{
    m_documents[documentId].copyPaths = paths;
    save();
}

void AppSettings::setLastDocumentId(const QString& documentId)
{
    m_lastDocumentId = documentId;
    save();
}

// --- This device -----------------------------------------------------------------

void AppSettings::setDisplayName(const QString& displayName)
{
    m_displayName = displayName;
    save();
}

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
    QJsonObject documents;
    if (!StrictJson::hasOnlyKeys(json,
                                 {KeyDocuments,
                                  KeyLastDocumentId,
                                  KeyDisplayName,
                                  KeyDeveloperExport,
                                  KeyServerAddress,
                                  KeyMap})
        || !StrictJson::readObject(json, KeyDocuments, documents)
        || !StrictJson::readOptionalNonEmptyString(json, KeyLastDocumentId, m_lastDocumentId)
        || !StrictJson::readOptionalNonEmptyString(json, KeyDisplayName, m_displayName)
        || !StrictJson::readOptionalNonEmptyString(json, KeyServerAddress, m_serverAddressOverride)
        || (json.contains(KeyDeveloperExport) && !StrictJson::readBool(json, KeyDeveloperExport, m_developerExport)))
    {
        return false;
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

    for (auto entry = documents.constBegin(); entry != documents.constEnd(); ++entry)
    {
        if (!entry.value().isObject())
        {
            return false;
        }

        const QJsonObject stateJson = entry.value().toObject();
        QJsonObject recordJson;
        QStringList copyPaths;
        DocumentState state;
        if (!StrictJson::hasOnlyKeys(stateJson, {KeyRecord, KeyWardName, KeyLastOpened, KeyCopyPaths})
            || !StrictJson::readObject(stateJson, KeyRecord, recordJson)
            || !StrictJson::readOptionalNonEmptyString(stateJson, KeyWardName, state.wardName)
            || (stateJson.contains(KeyCopyPaths) && !StrictJson::readStringArray(stateJson, KeyCopyPaths, state.copyPaths)))
        {
            return false;
        }

        if (stateJson.contains(KeyLastOpened))
        {
            QDateTime lastOpened;
            if (!StrictJson::readTime(stateJson, KeyLastOpened, lastOpened))
            {
                return false;
            }
            state.lastOpened = lastOpened;
        }

        const std::optional<DocumentRecord> record = DocumentRecord::fromJson(recordJson);
        if (!record.has_value())
        {
            return false;
        }
        state.record = record.value();
        m_documents.insert(entry.key(), state);
    }

    return true;
}

QByteArray AppSettings::write() const
{
    QJsonObject documents;
    for (auto entry = m_documents.constBegin(); entry != m_documents.constEnd(); ++entry)
    {
        const DocumentState& state = entry.value();
        QJsonObject stateJson;
        stateJson[KeyRecord] = state.record.toJson();
        if (!state.wardName.isEmpty())
        {
            stateJson[KeyWardName] = state.wardName;
        }
        if (state.lastOpened.has_value())
        {
            stateJson[KeyLastOpened] = StrictJson::writeTime(state.lastOpened.value());
        }
        if (!state.copyPaths.isEmpty())
        {
            stateJson[KeyCopyPaths] = QJsonArray::fromStringList(state.copyPaths);
        }
        documents[entry.key()] = stateJson;
    }

    QJsonObject json;
    json[KeyDocuments] = documents;
    if (!m_lastDocumentId.isEmpty())
    {
        json[KeyLastDocumentId] = m_lastDocumentId;
    }
    if (!m_displayName.isEmpty())
    {
        json[KeyDisplayName] = m_displayName;
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

void AppSettings::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());

    QSaveFile file(m_path);
    const QByteArray bytes = write();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        qWarning() << "Settings could not be written to" << m_path << ":" << file.errorString();
    }
}
