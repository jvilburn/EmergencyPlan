#pragma once

#include "WardStore.h"

#include <QByteArray>
#include <QHash>

/// A ward store in memory, which a test can make fail to write or to read.
class FakeWardStore : public WardStore
{
public:
    FakeWardStore() = default;

    bool write(const DocumentId& document, const QByteArray& bytes, QString* problem) override;
    StoreRead read(const DocumentId& document) const override;
    QList<DocumentId> documents() const override { return m_files.keys(); }

    /// While set, every write fails with "the disk is full" and leaves the file as it was.
    void setFailWrites(bool fail) { m_failWrites = fail; }

    /// While set, every read is a fault, "the disk did not answer".
    void setFailReads(bool fail) { m_failReads = fail; }

    QByteArray file(const DocumentId& document) const { return m_files.value(document); }
    void put(const DocumentId& document, const QByteArray& bytes) { m_files.insert(document, bytes); }

    /// How many writes have succeeded.
    int writes() const { return m_writes; }

private:
    QHash<DocumentId, QByteArray> m_files;
    bool m_failWrites = false;
    bool m_failReads = false;
    int m_writes = 0;
};
