#include "JsonService.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>

JsonResult JsonService::loadDocument(const QString& filePath)
{
    JsonResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        result.errorMessage = QObject::tr("Failed to open file: %1").arg(file.errorString());
        return result;
    }

    QString jsonString = QString::fromUtf8(file.readAll());
    file.close();

    return parseJson(jsonString);
}

bool JsonService::saveDocument(const QString& filePath, const Document& document, QString* errorMessage)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("Failed to open file for writing: %1").arg(file.errorString());
        }
        return false;
    }

    QString jsonString = toJsonString(document);
    QByteArray data = jsonString.toUtf8();

    if (file.write(data) != data.size())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("Failed to write data: %1").arg(file.errorString());
        }
        file.cancelWriting();
        return false;
    }

    if (!file.commit())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("Failed to commit file: %1").arg(file.errorString());
        }
        return false;
    }

    return true;
}

JsonResult JsonService::parseJson(const QString& jsonString)
{
    JsonResult result;

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = QObject::tr("JSON parse error at offset %1: %2")
            .arg(parseError.offset)
            .arg(parseError.errorString());
        return result;
    }

    if (!jsonDoc.isObject())
    {
        result.errorMessage = QObject::tr("Invalid JSON: root must be an object");
        return result;
    }

    QJsonObject json = jsonDoc.object();

    // Check version
    QString version = json["version"].toString();
    if (version.isEmpty())
    {
        result.errorMessage = QObject::tr("Invalid JSON file: missing version");
        return result;
    }

    // Parse document
    result.document = Document::fromJson(json);
    result.success = true;

    return result;
}

QString JsonService::toJsonString(const Document& document)
{
    QJsonObject json = document.toJson();

    // Add version info
    json["version"] = QString(CurrentVersion);

    // Add metadata
    QJsonObject metadata;
    metadata["lastModified"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    metadata["exportedBy"] = "Emergency Plan v1.0";
    json["metadata"] = metadata;

    QJsonDocument jsonDoc(json);
    return QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Indented));
}
