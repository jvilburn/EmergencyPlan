#pragma once

#include "CryptoTypes.h"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

/// What this device keeps about one document besides its files.
struct DocumentRecord
{
    /// The server version the base copy holds. Set once the ward is shared, and
    /// what makes it shared.
    std::optional<qint64> baseVersion;

    /// The base copy's payload hash, so a damaged or replaced base copy is not
    /// taken for the version the server and this device agreed on.
    QByteArray baseHash;

    /// The base copy's save counter. A local file above it has unpublished changes.
    qint64 baseSaveCounter = 0;

    /// The roster as the access notices last showed it and had it dismissed, so a
    /// notice shows only what changed since - across syncs, ward switches, and runs.
    /// Empty until the ward is first shown.
    QJsonObject rosterShown;

    QJsonObject toJson() const;

    /// Exactly what toJson writes; nullopt for anything else.
    static std::optional<DocumentRecord> fromJson(const QJsonObject& json);

    bool operator==(const DocumentRecord& other) const;
    bool operator!=(const DocumentRecord& other) const { return !(*this == other); }
};

/// Where records are kept. AppSettings keeps them in the app; tests keep them in
/// memory.
class DocumentRecords
{
public:
    virtual ~DocumentRecords() = default;

    /// A default record when none is held.
    virtual DocumentRecord record(const QString& documentId) const = 0;
    virtual void setRecord(const QString& documentId, const DocumentRecord& record) = 0;
    virtual void removeRecord(const QString& documentId) = 0;

    /// Every document with a record.
    virtual QStringList documentIds() const = 0;
};

/// Where one document's files live. DocumentLibrary names them.
struct DocumentPaths
{
    QString file;
    QString base;
};
