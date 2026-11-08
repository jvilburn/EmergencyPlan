#include "FakeWardStore.h"

#include <algorithm>

bool FakeWardStore::write(const DocumentId& document, const QByteArray& bytes, QString* problem)
{
    if (m_failWrites)
    {
        *problem = "the disk is full";
        return false;
    }
    m_files.insert(document, bytes);
    ++m_writes;
    return true;
}

StoreRead FakeWardStore::read(const DocumentId& document) const
{
    if (m_failReads)
    {
        return StoreRead{StoreReadStatus::Fault, QByteArray(), "the disk did not answer"};
    }
    if (!m_files.contains(document))
    {
        return StoreRead{StoreReadStatus::Missing, QByteArray(), QString()};
    }
    return StoreRead{StoreReadStatus::Read, m_files.value(document), QString()};
}

QList<DocumentId> FakeWardStore::documents() const
{
    QList<DocumentId> documents = m_files.keys();
    std::sort(documents.begin(), documents.end(), [](const DocumentId& a, const DocumentId& b) { return b.toString() < a.toString(); });
    return documents;
}
