#pragma once

#include "Id.h"

#include <QByteArray>
#include <QList>
#include <QString>

enum class StoreReadStatus
{
    Read,
    Missing,  ///< this device holds no file for the ward
    Fault     ///< the bytes could not be read at all
};

struct StoreRead
{
    StoreReadStatus status;
    QByteArray bytes;  ///< when Read
    QString problem;   ///< when Fault, a clause with no final period
};

/// Where this device keeps each ward's file, by document id, as bytes. The app keeps them in
/// app data; a test keeps them in memory and can make writes or reads fail.
class WardStore
{
public:
    virtual ~WardStore() = default;

    /// Replaces the ward's file with bytes in one write: afterwards the file is the old one or
    /// the new one, never part of either. A failure says why in problem, as a clause with no
    /// final period.
    virtual bool write(const DocumentId& document, const QByteArray& bytes, QString* problem) = 0;

    virtual StoreRead read(const DocumentId& document) const = 0;

    /// The id of every ward this device holds a file for, in no particular order.
    virtual QList<DocumentId> documents() const = 0;
};
