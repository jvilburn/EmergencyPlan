#pragma once

#include "AccessList.h"
#include "Id.h"
#include "WardHeader.h"

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

    /// The version sealedFile and sealedPlaintext write: version 1, written by sixteen 0x77
    /// bytes.
    static VersionRank version();

    /// A file of document under dataKey, sealed to recipients and holding payload, built the
    /// way WardFile::write builds one, so a test can seal any payload or recipient list.
    static QByteArray sealedFile(const DocumentId& document, const QByteArray& dataKey,
                                 const QList<RecipientKeys>& recipients, const QJsonObject& payload);

    /// A file of document under dataKey, sealed to recipients, whose payload decrypts to
    /// plaintext, so a test can seal bytes that are not a compressed payload.
    static QByteArray sealedPlaintext(const DocumentId& document, const QByteArray& dataKey,
                                      const QList<RecipientKeys>& recipients, const QByteArray& plaintext);
};
