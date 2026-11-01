#include "EmergencyResponse.h"

#include <QJsonArray>
#include <QObject>

namespace
{

QString timeToJson(const QDateTime& time)
{
    return time.toString(Qt::ISODateWithMs);
}

QDateTime timeFromJson(const QJsonValue& value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
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

/// The method named by value, or none when value names none.
std::optional<ContactMethod> methodFromJson(const QJsonValue& value)
{
    QString name = value.toString();
    for (ContactMethod method : {ContactMethod::Phone, ContactMethod::Text, ContactMethod::Email,
                                 ContactMethod::Visit, ContactMethod::Other})
    {
        if (methodToJson(method) == name)
        {
            return method;
        }
    }
    return std::nullopt;
}

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

/// The outcome named by value, or none when value names none.
std::optional<ContactOutcome> outcomeFromJson(const QJsonValue& value)
{
    QString name = value.toString();
    for (ContactOutcome outcome : {ContactOutcome::OK, ContactOutcome::UnableToReach})
    {
        if (outcomeToJson(outcome) == name)
        {
            return outcome;
        }
    }
    return std::nullopt;
}

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

template<typename T>
QList<T> listFrom(const QJsonValue& value)
{
    QList<T> items;
    QJsonArray array = value.toArray();
    for (const QJsonValue& item : array)
    {
        items.append(T::fromJson(item.toObject()));
    }
    return items;
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

TaskNotification TaskNotification::fromJson(const QJsonObject& json)
{
    TaskNotification notification;
    notification.m_method = methodFromJson(json["method"]).value_or(ContactMethod::Phone);
    notification.m_timestamp = timeFromJson(json["timestamp"]);
    notification.m_notes = json["notes"].toString();
    return notification;
}

bool TaskNotification::operator==(const TaskNotification& other) const
{
    return m_method == other.m_method
        && m_timestamp == other.m_timestamp
        && m_notes == other.m_notes;
}

// === ContactAttempt ===

ContactAttempt ContactAttempt::create(const FamilyId& familyId,
                                      std::optional<ContactMethod> method,
                                      const std::optional<PersonId>& who,
                                      std::optional<ContactOutcome> outcome,
                                      const QString& notes)
{
    ContactAttempt attempt;
    attempt.m_id = ContactAttemptId::generate();
    attempt.m_familyId = familyId;
    attempt.m_method = method;
    attempt.m_who = who;
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
    if (m_method.has_value())
    {
        json["method"] = methodToJson(*m_method);
    }
    if (m_who.has_value())
    {
        json["who"] = m_who->toString();
    }
    if (m_outcome.has_value())
    {
        json["outcome"] = outcomeToJson(*m_outcome);
    }
    json["timestamp"] = timeToJson(m_timestamp);
    writeNotes(json, m_notes);
    return json;
}

ContactAttempt ContactAttempt::fromJson(const QJsonObject& json)
{
    ContactAttempt attempt;
    attempt.m_id = ContactAttemptId::fromString(json["id"].toString());
    attempt.m_familyId = FamilyId::fromString(json["familyId"].toString());
    attempt.m_method = methodFromJson(json["method"]);
    if (json.contains("who"))
    {
        attempt.m_who = PersonId::fromString(json["who"].toString());
    }
    attempt.m_outcome = outcomeFromJson(json["outcome"]);
    attempt.m_timestamp = timeFromJson(json["timestamp"]);
    attempt.m_notes = json["notes"].toString();
    return attempt;
}

bool ContactAttempt::operator==(const ContactAttempt& other) const
{
    return m_id == other.m_id
        && m_familyId == other.m_familyId
        && m_method == other.m_method
        && m_who == other.m_who
        && m_outcome == other.m_outcome
        && m_timestamp == other.m_timestamp
        && m_notes == other.m_notes;
}

// === TaskAssignment ===

TaskAssignment TaskAssignment::toTeam(const TeamId& teamId, const QString& notes)
{
    TaskAssignment assignment;
    assignment.m_teamId = teamId;
    assignment.m_notes = notes;
    return assignment;
}

TaskAssignment TaskAssignment::toPerson(const PersonId& personId, const QString& notes)
{
    TaskAssignment assignment;
    assignment.m_personId = personId;
    assignment.m_notes = notes;
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
    writeNotes(json, m_notes);
    if (m_notification.has_value())
    {
        json["notification"] = m_notification->toJson();
    }
    return json;
}

TaskAssignment TaskAssignment::fromJson(const QJsonObject& json)
{
    TaskAssignment assignment;
    if (json.contains("teamId"))
    {
        assignment.m_teamId = TeamId::fromString(json["teamId"].toString());
    }
    if (json.contains("personId"))
    {
        assignment.m_personId = PersonId::fromString(json["personId"].toString());
    }
    assignment.m_notes = json["notes"].toString();
    if (json.contains("notification"))
    {
        assignment.m_notification = TaskNotification::fromJson(json["notification"].toObject());
    }
    return assignment;
}

bool TaskAssignment::operator==(const TaskAssignment& other) const
{
    return m_teamId == other.m_teamId
        && m_personId == other.m_personId
        && m_notes == other.m_notes
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

TaskResolution TaskResolution::fromJson(const QJsonObject& json)
{
    TaskResolution resolution;
    resolution.m_notes = json["notes"].toString();
    resolution.m_resolvedAt = timeFromJson(json["resolvedAt"]);
    return resolution;
}

bool TaskResolution::operator==(const TaskResolution& other) const
{
    return m_notes == other.m_notes
        && m_resolvedAt == other.m_resolvedAt;
}

// === ResponseTask ===

ResponseTask ResponseTask::create(const FamilyId& familyId, const QString& category, const QString& description)
{
    ResponseTask task;
    task.m_id = TaskId::generate();
    task.m_familyId = familyId;
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

ResponseTask ResponseTask::fromJson(const QJsonObject& json)
{
    ResponseTask task;
    task.m_id = TaskId::fromString(json["id"].toString());
    task.m_familyId = FamilyId::fromString(json["familyId"].toString());
    task.m_category = json["category"].toString();
    task.m_description = json["description"].toString();
    task.m_createdAt = timeFromJson(json["createdAt"]);
    if (json.contains("assignment"))
    {
        task.m_assignment = TaskAssignment::fromJson(json["assignment"].toObject());
    }
    if (json.contains("resolution"))
    {
        task.m_resolution = TaskResolution::fromJson(json["resolution"].toObject());
    }
    return task;
}

bool ResponseTask::operator==(const ResponseTask& other) const
{
    return m_id == other.m_id
        && m_familyId == other.m_familyId
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
    response.m_id = EmergencyResponseId::generate();
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

EffectiveContactStatus EmergencyResponse::statusOf(const FamilyId& familyId) const
{
    for (const ResponseTask& task : m_tasks)
    {
        if (task.familyId() == familyId && !task.isResolved())
        {
            return EffectiveContactStatus::NeedsHelp;
        }
    }
    for (const ContactAttempt& attempt : m_contactAttempts)
    {
        if (attempt.familyId() == familyId && attempt.outcome().has_value())
        {
            return *attempt.outcome() == ContactOutcome::OK
                ? EffectiveContactStatus::OK
                : EffectiveContactStatus::UnableToReach;
        }
    }
    return EffectiveContactStatus::NotContacted;
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
    json["id"] = m_id.toString();
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

EmergencyResponse EmergencyResponse::fromJson(const QJsonObject& json)
{
    EmergencyResponse response;
    response.m_id = EmergencyResponseId::fromString(json["id"].toString());
    response.m_name = json["name"].toString();
    response.m_startedAt = timeFromJson(json["startedAt"]);
    QJsonArray categories = json["taskCategories"].toArray();
    for (const QJsonValue& category : categories)
    {
        response.m_taskCategories.append(category.toString());
    }
    response.m_contactAttempts = listFrom<ContactAttempt>(json["contactAttempts"]);
    response.m_tasks = listFrom<ResponseTask>(json["tasks"]);
    return response;
}

bool EmergencyResponse::operator==(const EmergencyResponse& other) const
{
    return m_id == other.m_id
        && m_name == other.m_name
        && m_startedAt == other.m_startedAt
        && m_taskCategories == other.m_taskCategories
        && m_contactAttempts == other.m_contactAttempts
        && m_tasks == other.m_tasks;
}
