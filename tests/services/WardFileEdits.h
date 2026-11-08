#pragma once

#include "AccessList.h"
#include "Id.h"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QStringList>

/// Building and changing ward files in tests.
class WardFileEdits
{
public:
    /// The JSON object of file's header line.
    static QJsonObject headerOf(const QByteArray& file);

    /// file with its header line replaced by the compact form of header.
    static QByteArray withHeader(const QByteArray& file, const QJsonObject& header);

    /// json with the value at path set to value, or removed when value is undefined. A step of
    /// path that is a number indexes an array.
    static QJsonObject edited(const QJsonObject& json, const QStringList& path, const QJsonValue& value);

    /// A file of document under dataKey, sealed to recipients and holding payload, built the
    /// way WardFile::write builds one, so a test can seal any payload or recipient list.
    static QByteArray sealedFile(const DocumentId& document, const QByteArray& dataKey,
                                 const QList<RecipientKeys>& recipients, const QJsonObject& payload);
};
