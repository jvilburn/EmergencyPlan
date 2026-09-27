#include "ResponseMerge.h"

#include "AccessRoster.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace
{

/// The rule for one value: unchanged on theirs is mine, unchanged on mine is
/// theirs, and changed on both is mine - which is also right when both changed it
/// to the same thing.
template<typename T>
T mergeValue(const T& base, const T& mine, const T& theirs)
{
    if (theirs == base)
    {
        return mine;
    }
    if (mine == base)
    {
        return theirs;
    }
    return mine;
}

/// Presence of one keyed item - a family record or a task - three ways. Added on
/// either side is kept; deleted on one side while unchanged on the other is gone;
/// deleted on one side and edited on the other keeps the edit. Present on both is
/// merged against the base, or against emptyBase when the base never had it.
template<typename T, typename MergeBoth>
std::optional<T> mergePresence(const std::optional<T>& base,
                               const std::optional<T>& mine,
                               const std::optional<T>& theirs,
                               const T& emptyBase,
                               MergeBoth mergeBoth)
{
    if (mine.has_value() && theirs.has_value())
    {
        return mergeBoth(base.value_or(emptyBase), mine.value(), theirs.value());
    }

    if (!base.has_value())
    {
        return mine.has_value() ? mine : theirs;
    }

    if (mine.has_value())
    {
        return mine.value() == base.value() ? std::nullopt : mine;
    }

    if (theirs.has_value())
    {
        return theirs.value() == base.value() ? std::nullopt : theirs;
    }

    return std::nullopt;
}

template<typename T>
std::optional<T> lookup(const QHash<QString, T>& items, const QString& key)
{
    const auto found = items.constFind(key);
    return found == items.constEnd() ? std::nullopt : std::optional<T>(found.value());
}

/// Keys in a stable order: mine's first, as mine lists them, then theirs' others.
template<typename T, typename KeyOf>
QStringList orderedKeys(const QList<T>& mine, const QList<T>& theirs, KeyOf keyOf)
{
    QStringList keys;
    QSet<QString> seen;
    for (const QList<T>* list : {&mine, &theirs})
    {
        for (const T& item : *list)
        {
            const QString key = keyOf(item);
            if (!seen.contains(key))
            {
                seen.insert(key);
                keys.append(key);
            }
        }
    }
    return keys;
}

template<typename T, typename KeyOf>
QHash<QString, T> byKey(const QList<T>& items, KeyOf keyOf)
{
    QHash<QString, T> map;
    for (const T& item : items)
    {
        map.insert(keyOf(item), item);
    }
    return map;
}

QString attemptKey(const ContactAttempt& attempt)
{
    return attempt.id().toString();
}

QString taskKey(const ResponseTask& task)
{
    return task.id().toString();
}

/// Most recent first, as the model keeps them. Stable, so items recorded in the
/// same millisecond keep the order they arrived in.
bool attemptIsNewer(const ContactAttempt& left, const ContactAttempt& right)
{
    return left.timestamp() > right.timestamp();
}

bool taskIsNewer(const ResponseTask& left, const ResponseTask& right)
{
    return left.createdAt() > right.createdAt();
}

// --- Contact attempts ------------------------------------------------------

ContactAttempt mergeAttempt(const std::optional<ContactAttempt>& base,
                            const ContactAttempt& mine,
                            const ContactAttempt& theirs)
{
    const ContactAttempt reference = base.value_or(mine);

    ContactAttempt merged = mine;
    merged.setMethod(mergeValue(reference.method(), mine.method(), theirs.method()));
    merged.setNotes(mergeValue(reference.notes(), mine.notes(), theirs.notes()));
    return merged;
}

/// The union of both sides: an attempt is a record that someone was contacted, and
/// none is ever lost to a merge.
QList<ContactAttempt> mergeAttempts(const QList<ContactAttempt>& base,
                                    const QList<ContactAttempt>& mine,
                                    const QList<ContactAttempt>& theirs)
{
    const QHash<QString, ContactAttempt> baseByKey = byKey(base, attemptKey);
    const QHash<QString, ContactAttempt> mineByKey = byKey(mine, attemptKey);
    const QHash<QString, ContactAttempt> theirsByKey = byKey(theirs, attemptKey);

    QList<ContactAttempt> merged;
    for (const QString& key : orderedKeys(mine, theirs, attemptKey))
    {
        const std::optional<ContactAttempt> fromMine = lookup(mineByKey, key);
        const std::optional<ContactAttempt> fromTheirs = lookup(theirsByKey, key);

        if (fromMine.has_value() && fromTheirs.has_value())
        {
            merged.append(mergeAttempt(lookup(baseByKey, key), fromMine.value(), fromTheirs.value()));
        }
        else
        {
            merged.append(fromMine.has_value() ? fromMine.value() : fromTheirs.value());
        }
    }

    std::stable_sort(merged.begin(), merged.end(), attemptIsNewer);
    return merged;
}

// --- Tasks -----------------------------------------------------------------

/// Team, person, and notes belong together, so they merge as one value.
struct Assignment
{
    std::optional<TeamId> team;
    std::optional<PersonId> person;
    QString notes;

    bool operator==(const Assignment& other) const
    {
        return team == other.team && person == other.person && notes == other.notes;
    }
};

Assignment assignmentOf(const ResponseTask& task)
{
    return Assignment{task.assignedTeamId(), task.assignedPersonId(), task.assignmentNotes()};
}

/// Resolved, notes, and time belong together, so they merge as one value.
struct Resolution
{
    bool resolved = false;
    QString notes;
    std::optional<QDateTime> at;

    bool operator==(const Resolution& other) const
    {
        return resolved == other.resolved && notes == other.notes && at == other.at;
    }
};

Resolution resolutionOf(const ResponseTask& task)
{
    return Resolution{task.isResolved(), task.resolutionNotes(), task.resolvedAt()};
}

ResponseTask mergeTask(const ResponseTask& base, const ResponseTask& mine, const ResponseTask& theirs)
{
    ResponseTask merged = mine;
    merged.setCategory(mergeValue(base.category(), mine.category(), theirs.category()));
    merged.setDescription(mergeValue(base.description(), mine.description(), theirs.description()));

    // Assignment before notification: clearing an assignment also clears the
    // notification, which is then set from its own merge.
    const Assignment assignment =
        mergeValue(assignmentOf(base), assignmentOf(mine), assignmentOf(theirs));
    if (assignment.team.has_value())
    {
        merged.assignToTeam(assignment.team.value(), assignment.notes);
    }
    else if (assignment.person.has_value())
    {
        merged.assignToPerson(assignment.person.value(), assignment.notes);
    }
    else
    {
        merged.clearAssignment();
    }

    merged.setNotification(mergeValue(base.notification(), mine.notification(), theirs.notification()));

    const Resolution resolution =
        mergeValue(resolutionOf(base), resolutionOf(mine), resolutionOf(theirs));
    merged.setResolution(resolution.resolved, resolution.notes, resolution.at);

    return merged;
}

QList<ResponseTask> mergeTasks(const QList<ResponseTask>& base,
                               const QList<ResponseTask>& mine,
                               const QList<ResponseTask>& theirs)
{
    const QHash<QString, ResponseTask> baseByKey = byKey(base, taskKey);
    const QHash<QString, ResponseTask> mineByKey = byKey(mine, taskKey);
    const QHash<QString, ResponseTask> theirsByKey = byKey(theirs, taskKey);

    // Every key that could survive; one only in the base survives nowhere.
    QList<ResponseTask> merged;
    for (const QString& key : orderedKeys(mine, theirs, taskKey))
    {
        const std::optional<ResponseTask> result = mergePresence(lookup(baseByKey, key),
                                                                 lookup(mineByKey, key),
                                                                 lookup(theirsByKey, key),
                                                                 ResponseTask(),
                                                                 mergeTask);
        if (result.has_value())
        {
            merged.append(result.value());
        }
    }

    std::stable_sort(merged.begin(), merged.end(), taskIsNewer);
    return merged;
}

// --- Family records --------------------------------------------------------

FamilyResponseRecord mergeRecord(const FamilyResponseRecord& base,
                                 const FamilyResponseRecord& mine,
                                 const FamilyResponseRecord& theirs)
{
    FamilyResponseRecord merged = mine;
    merged.setDisplayName(mergeValue(base.displayName(), mine.displayName(), theirs.displayName()));
    merged.setAddress(mergeValue(base.address(), mine.address(), theirs.address()));
    merged.setContactStatus(mergeValue(base.contactStatus(), mine.contactStatus(), theirs.contactStatus()));
    merged.setContactAttempts(
        mergeAttempts(base.contactAttempts(), mine.contactAttempts(), theirs.contactAttempts()));
    merged.setTasks(mergeTasks(base.tasks(), mine.tasks(), theirs.tasks()));
    return merged;
}

std::optional<FamilyResponseRecord> recordFor(const EmergencyResponse& response, const FamilyId& family)
{
    const FamilyResponseRecord* record = response.findRecord(family);
    return record == nullptr ? std::nullopt : std::optional<FamilyResponseRecord>(*record);
}

QHash<FamilyId, FamilyResponseRecord> mergeRecords(const EmergencyResponse& base,
                                                   const EmergencyResponse& mine,
                                                   const EmergencyResponse& theirs)
{
    QSet<FamilyId> families;
    for (const EmergencyResponse* response : {&mine, &theirs})
    {
        for (auto record = response->familyRecords().constBegin();
             record != response->familyRecords().constEnd();
             ++record)
        {
            families.insert(record.key());
        }
    }

    QHash<FamilyId, FamilyResponseRecord> merged;
    for (const FamilyId& family : families)
    {
        const std::optional<FamilyResponseRecord> result =
            mergePresence(recordFor(base, family),
                          recordFor(mine, family),
                          recordFor(theirs, family),
                          FamilyResponseRecord::create(family, QString(), QString()),
                          mergeRecord);
        if (result.has_value())
        {
            merged.insert(family, result.value());
        }
    }
    return merged;
}

// --- Categories ------------------------------------------------------------

/// A set: kept when every side that had it in the base still has it, and added
/// when either side added it. Theirs' additions come before mine's.
QStringList mergeCategories(const QStringList& base, const QStringList& mine, const QStringList& theirs)
{
    QStringList merged;
    for (const QString& category : base)
    {
        if (mine.contains(category) && theirs.contains(category))
        {
            merged.append(category);
        }
    }

    for (const QStringList* side : {&theirs, &mine})
    {
        for (const QString& category : *side)
        {
            if (!base.contains(category) && !merged.contains(category))
            {
                merged.append(category);
            }
        }
    }
    return merged;
}

// --- The response ----------------------------------------------------------

EmergencyResponse mergeResponse(const EmergencyResponse& base,
                                const EmergencyResponse& mine,
                                const EmergencyResponse& theirs)
{
    EmergencyResponse merged = mine;
    merged.setName(mergeValue(base.name(), mine.name(), theirs.name()));
    merged.setStartedAt(mergeValue(base.startedAt(), mine.startedAt(), theirs.startedAt()));
    merged.setEndedAt(mergeValue(base.endedAt(), mine.endedAt(), theirs.endedAt()));
    merged.setTaskCategories(
        mergeCategories(base.taskCategories(), mine.taskCategories(), theirs.taskCategories()));
    merged.setFamilyRecords(mergeRecords(base, mine, theirs));
    return merged;
}

/// Started on both sides with no response in the base: one response, named and
/// dated by theirs, which reached the server first.
EmergencyResponse mergeStartedOnBoth(const EmergencyResponse& mine, const EmergencyResponse& theirs)
{
    EmergencyResponse merged = mergeResponse(EmergencyResponse(), mine, theirs);
    merged.setName(theirs.name());
    merged.setStartedAt(theirs.startedAt());
    return merged;
}

/// The document with everything the directory does not include taken out.
Document directoryOf(const Document& document)
{
    Document directory = document;
    directory.setEmergencyResponse(std::nullopt);
    directory.setAccessRoster(AccessRoster());
    return directory;
}

} // namespace

namespace ResponseMerge
{

bool directoryChanged(const Document& doc, const Document& base)
{
    return !(directoryOf(doc) == directoryOf(base));
}

std::optional<EmergencyResponse> merge(const std::optional<EmergencyResponse>& base,
                                       const std::optional<EmergencyResponse>& mine,
                                       const std::optional<EmergencyResponse>& theirs)
{
    if (!base.has_value() && mine.has_value() && theirs.has_value())
    {
        return mergeStartedOnBoth(mine.value(), theirs.value());
    }

    return mergePresence(base, mine, theirs, EmergencyResponse(), mergeResponse);
}

std::optional<Document> mergeDocuments(const Document& base, const Document& mine, const Document& theirs)
{
    const bool mineChanged = directoryChanged(mine, base);
    const bool theirsChanged = directoryChanged(theirs, base);

    if (mineChanged && theirsChanged && !(directoryOf(mine) == directoryOf(theirs)))
    {
        return std::nullopt;
    }

    Document merged = mineChanged ? mine : theirs;
    merged.setAccessRoster(theirs.accessRoster());
    merged.setEmergencyResponse(
        merge(base.emergencyResponse(), mine.emergencyResponse(), theirs.emergencyResponse()));
    return merged;
}

} // namespace ResponseMerge
