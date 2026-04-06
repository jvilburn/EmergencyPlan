#pragma once

#include <QString>
#include <QJsonObject>
#include "Document.h"

// Result type for JSON operations
struct JsonResult
{
    bool success = false;
    QString errorMessage;
    Document document;
};

class JsonService
{
public:
    // Current file format version
    static constexpr const char* CurrentVersion = "1.0";

    // Load document from file
    static JsonResult loadDocument(const QString& filePath);

    // Save document to file
    static bool saveDocument(const QString& filePath, const Document& document, QString* errorMessage);

    // Parse JSON string to document
    static JsonResult parseJson(const QString& jsonString);

    // Serialize document to JSON string (with indentation)
    static QString toJsonString(const Document& document);
};
