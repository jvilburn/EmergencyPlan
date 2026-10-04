#pragma once

#include "Reconcile.h"
#include "RosterChange.h"

#include <QByteArray>
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

    /// The newest server version this device has taken in or published.
    Reconcile::HighWaterMark highWaterMark;

    /// Roster changes waiting to publish, oldest first.
    QList<RosterChange> pendingChanges;

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

    /// Every document with a record, so the roster change queue can resume the
    /// changes kept in them.
    virtual QStringList documentIds() const = 0;
};

/// Where one document's files live. DocumentLibrary names them.
struct DocumentPaths
{
    QString file;
    QString base;
};
