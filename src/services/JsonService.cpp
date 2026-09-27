#include "JsonService.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>

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

    const std::optional<Document> document = Document::parse(jsonDoc.object());
    if (!document.has_value())
    {
        result.errorMessage = QObject::tr("This document is damaged or incomplete, so it was not opened.");
        return result;
    }

    result.document = document.value();
    result.success = true;

    return result;
}

QString JsonService::toJsonString(const Document& document)
{
    QJsonDocument jsonDoc(document.toJson());
    return QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Indented));
}
