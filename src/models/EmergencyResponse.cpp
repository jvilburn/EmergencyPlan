#include "EmergencyResponse.h"

#include <QJsonArray>
#include <QObject>

namespace
{

QString timeToJson(const QDateTime& time)
{
    return time.toString(Qt::ISODateWithMs);
}

QString methodToJson(ContactMethod method)
{
    switch (method)
    {
    case ContactMethod::Phone:
        return "phone";
    case ContactMethod::Text:
        return "text";
    case ContactMethod::Email:
        return "email";
    case ContactMethod::Visit:
        return "visit";
    case ContactMethod::Other:
        return "other";
    }
    return QString();
}

const QList<ContactMethod> AllMethods = {ContactMethod::Phone, ContactMethod::Text, ContactMethod::Email,
                                         ContactMethod::Visit, ContactMethod::Other};

QString outcomeToJson(ContactOutcome outcome)
{
    switch (outcome)
    {
    case ContactOutcome::OK:
        return "ok";
    case ContactOutcome::UnableToReach:
        return "unableToReach";
    }
    return QString();
}

const QList<ContactOutcome> AllOutcomes = {ContactOutcome::OK, ContactOutcome::UnableToReach};

/// Writes notes under "notes" when there are any.
void writeNotes(QJsonObject& json, const QString& notes)
{
    if (!notes.isEmpty())
    {
        json["notes"] = notes;
    }
}

/// True when a comes before b in the most-recent-first order: a later time, or the same
/// time and a greater id.
template<typename Id>
bool isMoreRecent(const QDateTime& aTime, const Id& aId, const QDateTime& bTime, const Id& bId)
{
    if (aTime != bTime)
    {
        return aTime > bTime;
    }
    return bId < aId;
}

bool attemptComesFirst(const ContactAttempt& a, const ContactAttempt& b)
{
    return isMoreRecent(a.timestamp(), a.id(), b.timestamp(), b.id());
}

bool taskComesFirst(const ResponseTask& a, const ResponseTask& b)
{
    return isMoreRecent(a.createdAt(), a.id(), b.createdAt(), b.id());
}

/// Inserts item before the first of items it comes first against.
template<typename T>
void insertInOrder(QList<T>& items, const T& item, bool (*comesFirst)(const T&, const T&))
{
    qsizetype index = 0;
    while (index < items.size() && !comesFirst(item, items[index]))
    {
        ++index;
    }
    items.insert(index, item);
}

/// The index of the item with that id, or -1 when items holds none.
template<typename T, typename Id>
qsizetype indexOfId(const QList<T>& items, const Id& id)
{
    for (qsizetype i = 0; i < items.size(); ++i)
    {
        if (items[i].id() == id)
        {
            return i;
        }
    }
    return -1;
}

/// The items that carry familyId, in their order.
template<typename T>
QList<T> itemsFor(const QList<T>& items, const FamilyId& familyId)
{
    QList<T> found;
    for (const T& item : items)
    {
        if (item.familyId() == familyId)
        {
            found.append(item);
        }
    }
    return found;
}

template<typename T>
QJsonArray arrayOf(const QList<T>& items)
{
    QJsonArray array;
    for (const T& item : items)
    {
        array.append(item.toJson());
    }
    return array;
}

}  // namespace

QString contactMethodName(ContactMethod method)
{
    switch (method)
    {
    case ContactMethod::Phone:
        return QObject::tr("Phone");
    case ContactMethod::Text:
        return QObject::tr("Text");
    case ContactMethod::Email:
        return QObject::tr("Email");
    case ContactMethod::Visit:
        return QObject::tr("Visit");
    case ContactMethod::Other:
        return QObject::tr("Other");
    }
    return QString();
}

QString contactOutcomeName(ContactOutcome outcome)
{
    switch (outcome)
    {
    case ContactOutcome::OK:
        return QObject::tr("OK");
    case ContactOutcome::UnableToReach:
        return QObject::tr("Unable to Reach");
    }
    return QString();
}

QString contactMarkName(ContactOutcome outcome)
{
    switch (outcome)
    {
    case ContactOutcome::OK:
        return QObject::tr("Marked OK");
    case ContactOutcome::UnableToReach:
        return QObject::tr("Marked Unable to Reach");
    }
    return QString();
}

QStringList contactAttemptParts(const ContactAttempt& attempt, const QString& timeText)
{
    QStringList parts = {timeText};
    std::optional<ContactMethod> method = attempt.method();
    std::optional<ContactOutcome> outcome = attempt.outcome();
    if (method.has_value())
    {
        parts.append(contactMethodName(*method));
    }
    if (!attempt.whoName().isEmpty())
    {
        parts.append(attempt.whoName());
    }
    if (outcome.has_value())
    {
        parts.append(method.has_value() ? contactOutcomeName(*outcome) : contactMarkName(*outcome));
    }
    if (!attempt.notes().isEmpty())
    {
        parts.append(attempt.notes());
    }
    return parts;
}

// === TaskNotification ===

TaskNotification TaskNotification::create(ContactMethod method, const QString& notes)
{
    TaskNotification notification;
    notification.m_method = method;
    notification.m_timestamp = QDateTime::currentDateTimeUtc();
    notification.m_notes = notes;
    return notification;
}

QJsonObject TaskNotification::toJson() const
{
    QJsonObject json;
    json["method"] = methodToJson(m_method);
    json["timestamp"] = timeToJson(m_timestamp);
    writeNotes(json, m_notes);
    return json;
}

TaskNotification TaskNotification::fromJson(JsonReader json)
{
    TaskNotification notification;
    notification.m_method = json.word("method", AllMethods, &methodToJson);
    notification.m_timestamp = json.time("timestamp");
    notification.m_notes = json.optionalString("notes");
    json.finish();
    return notification;
}

bool TaskNotification::operator==(const TaskNotification& other) const
{
    return m_method == other.m_method
        && m_timestamp == other.m_timestamp
        && m_notes == other.m_notes;
}

// === ContactAttempt ===

ContactAttempt ContactAttempt::create(const Family& family,
                                      std::optional<ContactMethod> method,
                                      const std::optional<Person>& who,
                                      std::optional<ContactOutcome> outcome,
                                      const QString& notes)
{
    ContactAttempt attempt;
    attempt.m_id = ContactAttemptId::generate();
    attempt.m_familyId = family.id();
    attempt.m_familyName = family.displayName();
    attempt.m_method = method;
    if (who.has_value())
    {
        attempt.m_who = who->id();
        attempt.m_whoName = who->displayName();
    }
    attempt.m_outcome = outcome;
    attempt.m_timestamp = QDateTime::currentDateTimeUtc();
    attempt.m_notes = notes;
    return attempt;
}

QJsonObject ContactAttempt::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["familyId"] = m_familyId.toString();
    json["familyName"] = m_familyName;
    if (m_method.has_value())
    {
        json["method"] = methodToJson(*m_method);
    }
    if (m_who.has_value())
    {
        json["who"] = m_who->toString();
    }
    if (!m_whoName.isEmpty())
    {
        json["whoName"] = m_whoName;
    }
    if (m_outcome.has_value())
    {
        json["outcome"] = outcomeToJson(*m_outcome);
    }
    json["timestamp"] = timeToJson(m_timestamp);
    writeNotes(json, m_notes);
    return json;
}

ContactAttempt ContactAttempt::fromJson(JsonReader json)
{
    ContactAttempt attempt;
    attempt.m_id = json.id<ContactAttemptId>("id");
    attempt.m_familyId = json.id<FamilyId>("familyId");
    attempt.m_familyName = json.string("familyName");
    attempt.m_method = json.optionalWord("method", AllMethods, &methodToJson);
    attempt.m_who = json.optionalId<PersonId>("who");
    attempt.m_whoName = json.optionalString("whoName");
    attempt.m_outcome = json.optionalWord("outcome", AllOutcomes, &outcomeToJson);
    attempt.m_timestamp = json.time("timestamp");
    attempt.m_notes = json.optionalString("notes");
    json.finish();
    return attempt;
}

bool ContactAttempt::operator==(const ContactAttempt& other) const
{
    return m_id == other.m_id
        && m_familyId == other.m_familyId
        && m_familyName == other.m_familyName
        && m_method == other.m_method
        && m_who == other.m_who
        && m_whoName == other.m_whoName
        && m_outcome == other.m_outcome
        && m_timestamp == other.m_timestamp
        && m_notes == other.m_notes;
}

// === TaskAssignment ===

TaskAssignment TaskAssignment::toTeam(const Team& team)
{
    TaskAssignment assignment;
    assignment.m_teamId = team.id();
    assignment.m_assigneeName = team.name();
    return assignment;
}

TaskAssignment TaskAssignment::toPerson(const Person& person)
{
    TaskAssignment assignment;
    assignment.m_personId = person.id();
    assignment.m_assigneeName = person.displayName();
    return assignment;
}

QJsonObject TaskAssignment::toJson() const
{
    QJsonObject json;
    if (m_teamId.has_value())
    {
        json["teamId"] = m_teamId->toString();
    }
    if (m_personId.has_value())
    {
        json["personId"] = m_personId->toString();
    }
    json["assigneeName"] = m_assigneeName;
    if (m_notification.has_value())
    {
        json["notification"] = m_notification->toJson();
    }
    return json;
}

TaskAssignment TaskAssignment::fromJson(JsonReader json)
{
    TaskAssignment assignment;
    assignment.m_teamId = json.optionalId<TeamId>("teamId");
    assignment.m_personId = json.optionalId<PersonId>("personId");
    assignment.m_assigneeName = json.string("assigneeName");
    std::optional<JsonReader> notification = json.optionalObject("notification");
    if (notification.has_value())
    {
        assignment.m_notification = TaskNotification::fromJson(*notification);
    }
    json.finish();
    return assignment;
}

bool TaskAssignment::operator==(const TaskAssignment& other) const
{
    return m_teamId == other.m_teamId
        && m_personId == other.m_personId
        && m_assigneeName == other.m_assigneeName
        && m_notification == other.m_notification;
}

// === TaskResolution ===

TaskResolution TaskResolution::create(const QString& notes)
{
    TaskResolution resolution;
    resolution.m_notes = notes;
    resolution.m_resolvedAt = QDateTime::currentDateTimeUtc();
    return resolution;
}

QJsonObject TaskResolution::toJson() const
{
    QJsonObject json;
    writeNotes(json, m_notes);
    json["resolvedAt"] = timeToJson(m_resolvedAt);
    return json;
}

TaskResolution TaskResolution::fromJson(JsonReader json)
{
    TaskResolution resolution;
    resolution.m_notes = json.optionalString("notes");
    resolution.m_resolvedAt = json.time("resolvedAt");
    json.finish();
    return resolution;
}

bool TaskResolution::operator==(const TaskResolution& other) const
{
    return m_notes == other.m_notes
        && m_resolvedAt == other.m_resolvedAt;
}

// === ResponseTask ===

ResponseTask ResponseTask::create(const Family& family, const QString& category, const QString& description)
{
    ResponseTask task;
    task.m_id = TaskId::generate();
    task.m_familyId = family.id();
    task.m_familyName = family.displayName();
    task.m_category = category;
    task.m_description = description;
    task.m_createdAt = QDateTime::currentDateTimeUtc();
    return task;
}

QJsonObject ResponseTask::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["familyId"] = m_familyId.toString();
    json["familyName"] = m_familyName;
    json["category"] = m_category;
    json["description"] = m_description;
    json["createdAt"] = timeToJson(m_createdAt);
    if (m_assignment.has_value())
    {
        json["assignment"] = m_assignment->toJson();
    }
    if (m_resolution.has_value())
    {
        json["resolution"] = m_resolution->toJson();
    }
    return json;
}

ResponseTask ResponseTask::fromJson(JsonReader json)
{
    ResponseTask task;
    task.m_id = json.id<TaskId>("id");
    task.m_familyId = json.id<FamilyId>("familyId");
    task.m_familyName = json.string("familyName");
    task.m_category = json.string("category");
    task.m_description = json.string("description");
    task.m_createdAt = json.time("createdAt");
    std::optional<JsonReader> assignment = json.optionalObject("assignment");
    if (assignment.has_value())
    {
        task.m_assignment = TaskAssignment::fromJson(*assignment);
    }
    std::optional<JsonReader> resolution = json.optionalObject("resolution");
    if (resolution.has_value())
    {
        task.m_resolution = TaskResolution::fromJson(*resolution);
    }
    json.finish();
    return task;
}

bool ResponseTask::operator==(const ResponseTask& other) const
{
    return m_id == other.m_id
        && m_familyId == other.m_familyId
        && m_familyName == other.m_familyName
        && m_category == other.m_category
        && m_description == other.m_description
        && m_createdAt == other.m_createdAt
        && m_assignment == other.m_assignment
        && m_resolution == other.m_resolution;
}

// === EmergencyResponse ===

EmergencyResponse EmergencyResponse::create(const QString& name)
{
    EmergencyResponse response;
    response.m_name = name;
    response.m_startedAt = QDateTime::currentDateTimeUtc();
    response.m_taskCategories = {QObject::tr("Tree removal"), QObject::tr("Flooding"), QObject::tr("Generator"),
                                 QObject::tr("Medical"), QObject::tr("Transport"), QObject::tr("Shelter"),
                                 QObject::tr("Other")};
    return response;
}

const ContactAttempt* EmergencyResponse::findContactAttempt(const ContactAttemptId& id) const
{
    qsizetype index = indexOfId(m_contactAttempts, id);
    return index < 0 ? nullptr : &m_contactAttempts[index];
}

const ResponseTask* EmergencyResponse::findTask(const TaskId& id) const
{
    qsizetype index = indexOfId(m_tasks, id);
    return index < 0 ? nullptr : &m_tasks[index];
}

void EmergencyResponse::insertContactAttempt(const ContactAttempt& attempt)
{
    insertInOrder(m_contactAttempts, attempt, &attemptComesFirst);
}

void EmergencyResponse::insertTask(const ResponseTask& task)
{
    insertInOrder(m_tasks, task, &taskComesFirst);
}

void EmergencyResponse::setTask(const ResponseTask& task)
{
    qsizetype index = indexOfId(m_tasks, task.id());
    if (index >= 0)
    {
        m_tasks[index] = task;
    }
}

void EmergencyResponse::removeContactAttempt(const ContactAttemptId& id)
{
    qsizetype index = indexOfId(m_contactAttempts, id);
    if (index >= 0)
    {
        m_contactAttempts.removeAt(index);
    }
}

void EmergencyResponse::removeTask(const TaskId& id)
{
    qsizetype index = indexOfId(m_tasks, id);
    if (index >= 0)
    {
        m_tasks.removeAt(index);
    }
}

void EmergencyResponse::addTaskCategory(const QString& category)
{
    m_taskCategories.append(category);
}

QSet<FamilyId> EmergencyResponse::familyIds() const
{
    QSet<FamilyId> ids;
    for (const ContactAttempt& attempt : m_contactAttempts)
    {
        ids.insert(attempt.familyId());
    }
    for (const ResponseTask& task : m_tasks)
    {
        ids.insert(task.familyId());
    }
    return ids;
}

QList<ContactAttempt> EmergencyResponse::contactAttemptsFor(const FamilyId& familyId) const
{
    return itemsFor(m_contactAttempts, familyId);
}

QList<ResponseTask> EmergencyResponse::tasksFor(const FamilyId& familyId) const
{
    return itemsFor(m_tasks, familyId);
}

QHash<FamilyId, EffectiveContactStatus> EmergencyResponse::statusByFamily() const
{
    QHash<FamilyId, EffectiveContactStatus> statuses;

    // Attempts are newest first, so the first outcome seen for a family is its latest.
    for (const ContactAttempt& attempt : m_contactAttempts)
    {
        if (!attempt.outcome().has_value() || statuses.contains(attempt.familyId()))
        {
            continue;
        }
        statuses.insert(attempt.familyId(),
                        *attempt.outcome() == ContactOutcome::OK
                            ? EffectiveContactStatus::OK
                            : EffectiveContactStatus::UnableToReach);
    }
    for (const ResponseTask& task : m_tasks)
    {
        if (!task.isResolved())
        {
            statuses.insert(task.familyId(), EffectiveContactStatus::NeedsHelp);
        }
    }
    return statuses;
}

QSet<FamilyId> EmergencyResponse::countedFamilies(const QHash<FamilyId, Family>& families) const
{
    QSet<FamilyId> ids = familyIds();
    for (auto it = families.constBegin(); it != families.constEnd(); ++it)
    {
        ids.insert(it.key());
    }
    return ids;
}

int EmergencyResponse::totalFamilies(const QHash<FamilyId, Family>& families) const
{
    return static_cast<int>(countedFamilies(families).size());
}

int EmergencyResponse::countByStatus(EffectiveContactStatus status, const QHash<FamilyId, Family>& families) const
{
    QHash<FamilyId, EffectiveContactStatus> statuses = statusByFamily();
    int count = 0;
    for (const FamilyId& familyId : countedFamilies(families))
    {
        if (statuses.value(familyId, EffectiveContactStatus::NotContacted) == status)
        {
            ++count;
        }
    }
    return count;
}

QJsonObject EmergencyResponse::toJson() const
{
    QJsonObject json;
    json["name"] = m_name;
    json["startedAt"] = timeToJson(m_startedAt);
    json["taskCategories"] = QJsonArray::fromStringList(m_taskCategories);
    if (!m_contactAttempts.isEmpty())
    {
        json["contactAttempts"] = arrayOf(m_contactAttempts);
    }
    if (!m_tasks.isEmpty())
    {
        json["tasks"] = arrayOf(m_tasks);
    }
    return json;
}

EmergencyResponse EmergencyResponse::fromJson(JsonReader json)
{
    EmergencyResponse response;
    response.m_name = json.string("name");
    response.m_startedAt = json.time("startedAt");
    response.m_taskCategories = json.strings("taskCategories");
    const QList<JsonReader> attempts = json.optionalObjects("contactAttempts");
    for (const JsonReader& attempt : attempts)
    {
        response.insertContactAttempt(ContactAttempt::fromJson(attempt));
    }
    const QList<JsonReader> tasks = json.optionalObjects("tasks");
    for (const JsonReader& task : tasks)
    {
        response.insertTask(ResponseTask::fromJson(task));
    }
    json.finish();
    return response;
}

bool EmergencyResponse::operator==(const EmergencyResponse& other) const
{
    return m_name == other.m_name
        && m_startedAt == other.m_startedAt
        && m_taskCategories == other.m_taskCategories
        && m_contactAttempts == other.m_contactAttempts
        && m_tasks == other.m_tasks;
}
