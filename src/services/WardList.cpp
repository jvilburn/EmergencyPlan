#include "WardList.h"

#include <algorithm>

namespace
{

/// Whether a lists before b: a ward that reads before one that does not, then by name, then
/// by id, so wards with one name keep one order.
bool listsBefore(const WardListEntry& a, const WardListEntry& b)
{
    bool aOpens = a.status == OpenStatus::Opened;
    bool bOpens = b.status == OpenStatus::Opened;
    if (aOpens != bOpens)
    {
        return aOpens;
    }
    int byName = QString::localeAwareCompare(a.name, b.name);
    if (byName != 0)
    {
        return byName < 0;
    }
    return a.document.toString() < b.document.toString();
}

}  // namespace

QList<WardListEntry> WardList::read(const Identity& identity, const WardStore& store)
{
    QList<WardListEntry> entries;
    for (const DocumentId& document : store.documents())
    {
        LocalRead local = WardSession::readLocal(store, identity, document);
        WardListEntry entry{document, local.result.status, QString(), false, false};
        if (local.payload)
        {
            entry.name = local.payload->ward.wardName();
            entry.changesToShare = WardSession::changesToShare(local.payload->device->journal, local.payload->landed,
                                                               identity.fingerprint());
            entry.noLongerShared = local.payload->device->noLongerShared;
        }
        entries.append(entry);
    }
    std::sort(entries.begin(), entries.end(), listsBefore);
    return entries;
}
