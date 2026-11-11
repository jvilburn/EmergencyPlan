#pragma once

#include "Id.h"
#include "Identity.h"
#include "WardSession.h"
#include "WardStore.h"

#include <QList>
#include <QString>

/// One ward this device holds, as the ward list shows it.
struct WardListEntry
{
    DocumentId document;
    OpenStatus status;    ///< Opened when the file reads; otherwise why it does not
    QString name;         ///< the ward's name, when the file reads
    bool changesToShare;  ///< its journal holds changes no version this device holds carries
    bool noLongerShared;  ///< the ward is no longer this device's
};

/// The wards this device holds, read from their local files.
class WardList
{
public:
    /// One entry per ward file in store, each read as identity reads it through
    /// WardSession::readLocal: the ones that read sorted by name, then the rest by id.
    static QList<WardListEntry> read(const Identity& identity, const WardStore& store);
};
