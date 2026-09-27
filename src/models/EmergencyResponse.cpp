#include "EmergencyResponse.h"

#include "StrictJson.h"

#include <QJsonArray>

// === Helper functions for enum serialization ===

static QString contactMethodToString(ContactMethod method)
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
    return "phone";
}

static std::optional<ContactMethod> contactMethodFromString(const QString& str)
{
    for (const ContactMethod method :
         {ContactMethod::Phone, ContactMethod::Text, ContactMethod::Email, ContactMethod::Visit, ContactMethod::Other})
    {
        if (str == contactMethodToString(method))
        {
            return method;
        }
    }
    return std::nullopt;
}

static QString contactStatusToString(ContactStatus status)
{
    switch (status)
    {
    case ContactStatus::NotContacted:
        return "notContacted";
    case ContactStatus::OK:
        return "ok";
    case ContactStatus::UnableToReach:
        return "unableToReach";
    }
    return "notContacted";
}

static std::optional<ContactStatus> contactStatusFromString(const QString& str)
{
    for (const ContactStatus status :
         {ContactStatus::NotContacted, ContactStatus::OK, ContactStatus::UnableToReach})
    {
        if (str == contactStatusToString(status))
        {
            return status;
        }
    }
    return std::nullopt;
}

/// A method field read strictly.
static bool readMethod(const QJsonObject& json, ContactMethod& out)
{
    QString text;
    if (!StrictJson::readString(json, QLatin1String("method"), text))
    {
        return false;
    }
    const std::optional<ContactMethod> method = contactMethodFromString(text);
    if (!method.has_value())
    {
        return false;
    }
    out = method.value();
    return true;
}

/// An optional notes field: written only when not empty.
static bool readNotes(const QJsonObject& json, QLatin1String key, QString& out)
{
    return !json.contains(key) || StrictJson::readNonEmptyString(json, key, out);
}

// === TaskNotification ===

TaskNotification TaskNotification::create(ContactMethod method, const QString& notes)
{
    TaskNotification n;
    n.m_method = method;
    n.m_timestamp = QDateTime::currentDateTimeUtc();
    n.m_notes = notes;
    return n;
}

QJsonObject TaskNotification::toJson() const
{
    QJsonObject json;
    json["method"] = contactMethodToString(m_method);
    json["timestamp"] = StrictJson::writeTime(m_timestamp);
    if (!m_notes.isEmpty())
    {
        json["notes"] = m_notes;
    }
    return json;
}

std::optional<TaskNotification> TaskNotification::fromJson(const QJsonObject& json)
{
    TaskNotification notification;
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("method"), QLatin1String("timestamp"), QLatin1String("notes")})
        || !readMethod(json, notification.m_method)
        || !StrictJson::readTime(json, QLatin1String("timestamp"), notification.m_timestamp)
        || !readNotes(json, QLatin1String("notes"), notification.m_notes))
    {
        return std::nullopt;
    }
    return notification;
}

bool TaskNotification::operator==(const TaskNotification& other) const
{
    return m_method == other.m_method
        && m_timestamp == other.m_timestamp
        && m_notes == other.m_notes;
}

// === ContactAttempt ===

ContactAttempt ContactAttempt::create(ContactMethod method, const PersonId& who, const QString& notes)
{
    Q_ASSERT(method != ContactMethod::Other || !notes.isEmpty());

    ContactAttempt attempt;
    attempt.m_id = ContactAttemptId::generate();
    attempt.m_method = method;
    attempt.m_who = who;
    attempt.m_timestamp = QDateTime::currentDateTimeUtc();
    attempt.m_notes = notes;
    return attempt;
}

QJsonObject ContactAttempt::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["method"] = contactMethodToString(m_method);
    json["who"] = m_who.toString();
    json["timestamp"] = StrictJson::writeTime(m_timestamp);
    if (!m_notes.isEmpty())
    {
        json["notes"] = m_notes;
    }
    return json;
}

std::optional<ContactAttempt> ContactAttempt::fromJson(const QJsonObject& json)
{
    ContactAttempt attempt;
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("id"),
                                  QLatin1String("method"),
                                  QLatin1String("who"),
                                  QLatin1String("timestamp"),
                                  QLatin1String("notes")})
        || !StrictJson::readId(json, QLatin1String("id"), attempt.m_id)
        || !readMethod(json, attempt.m_method)
        || !StrictJson::readId(json, QLatin1String("who"), attempt.m_who)
        || !StrictJson::readTime(json, QLatin1String("timestamp"), attempt.m_timestamp)
        || !readNotes(json, QLatin1String("notes"), attempt.m_notes))
    {
        return std::nullopt;
    }
    return attempt;
}

bool ContactAttempt::operator==(const ContactAttempt& other) const
{
    return m_id == other.m_id
        && m_method == other.m_method
        && m_who == other.m_who
        && m_timestamp == other.m_timestamp
        && m_notes == other.m_notes;
}

// === ResponseTask ===

ResponseTask ResponseTask::create(const QString& category, const QString& description)
{
    ResponseTask task;
    task.m_id = TaskId::generate();
    task.m_category = category;
    task.m_description = description;
    task.m_createdAt = QDateTime::currentDateTimeUtc();
    return task;
}

void ResponseTask::assignToTeam(const TeamId& teamId, const QString& notes)
{
    m_assignedTeamId = teamId;
    m_assignedPersonId.reset();
    m_assignmentNotes = notes;
}

void ResponseTask::assignToPerson(const PersonId& personId, const QString& notes)
{
    m_assignedPersonId = personId;
    m_assignedTeamId.reset();
    m_assignmentNotes = notes;
}

void ResponseTask::clearAssignment()
{
    m_assignedTeamId.reset();
    m_assignedPersonId.reset();
    m_assignmentNotes.clear();
    m_notification.reset();
}

void ResponseTask::setNotification(const std::optional<TaskNotification>& notification)
{
    m_notification = notification;
}

void ResponseTask::resolve(const QString& notes)
{
    m_resolved = true;
    m_resolutionNotes = notes;
    m_resolvedAt = QDateTime::currentDateTimeUtc();
}

void ResponseTask::reopen()
{
    m_resolved = false;
    m_resolutionNotes.clear();
    m_resolvedAt.reset();
}

void ResponseTask::setResolution(bool resolved,
                                 const QString& notes,
                                 const std::optional<QDateTime>& resolvedAt)
{
    m_resolved = resolved;
    m_resolutionNotes = notes;
    m_resolvedAt = resolvedAt;
}

bool ResponseTask::isAssigned() const
{
    return m_assignedTeamId.has_value() || m_assignedPersonId.has_value();
}

bool ResponseTask::isNotified() const
{
    return m_notification.has_value();
}

QJsonObject ResponseTask::toJson() const
{
    QJsonObject json;
    json["id"] = m_id.toString();
    json["category"] = m_category;
    json["description"] = m_description;
    json["createdAt"] = StrictJson::writeTime(m_createdAt);

    if (m_assignedTeamId.has_value())
    {
        json["assignedTeamId"] = m_assignedTeamId->toString();
    }
    if (m_assignedPersonId.has_value())
    {
        json["assignedPersonId"] = m_assignedPersonId->toString();
    }
    if (!m_assignmentNotes.isEmpty())
    {
        json["assignmentNotes"] = m_assignmentNotes;
    }
    if (m_notification.has_value())
    {
        json["notification"] = m_notification->toJson();
    }

    json["resolved"] = m_resolved;
    if (!m_resolutionNotes.isEmpty())
    {
        json["resolutionNotes"] = m_resolutionNotes;
    }
    if (m_resolvedAt.has_value())
    {
        json["resolvedAt"] = StrictJson::writeTime(m_resolvedAt.value());
    }

    return json;
}

std::optional<ResponseTask> ResponseTask::fromJson(const QJsonObject& json)
{
    ResponseTask task;
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("id"),
                                  QLatin1String("category"),
                                  QLatin1String("description"),
                                  QLatin1String("createdAt"),
                                  QLatin1String("assignedTeamId"),
                                  QLatin1String("assignedPersonId"),
                                  QLatin1String("assignmentNotes"),
                                  QLatin1String("notification"),
                                  QLatin1String("resolved"),
                                  QLatin1String("resolutionNotes"),
                                  QLatin1String("resolvedAt")})
        || !StrictJson::readId(json, QLatin1String("id"), task.m_id)
        || !StrictJson::readString(json, QLatin1String("category"), task.m_category)
        || !StrictJson::readString(json, QLatin1String("description"), task.m_description)
        || !StrictJson::readTime(json, QLatin1String("createdAt"), task.m_createdAt)
        || !StrictJson::readBool(json, QLatin1String("resolved"), task.m_resolved)
        || !readNotes(json, QLatin1String("assignmentNotes"), task.m_assignmentNotes)
        || !readNotes(json, QLatin1String("resolutionNotes"), task.m_resolutionNotes))
    {
        return std::nullopt;
    }

    // At most one assignee: assigning to a team clears the person and the reverse.
    if (json.contains("assignedTeamId") && json.contains("assignedPersonId"))
    {
        return std::nullopt;
    }

    if (json.contains("assignedTeamId"))
    {
        TeamId team;
        if (!StrictJson::readId(json, QLatin1String("assignedTeamId"), team))
        {
            return std::nullopt;
        }
        task.m_assignedTeamId = team;
    }

    if (json.contains("assignedPersonId"))
    {
        PersonId person;
        if (!StrictJson::readId(json, QLatin1String("assignedPersonId"), person))
        {
            return std::nullopt;
        }
        task.m_assignedPersonId = person;
    }

    if (json.contains("notification"))
    {
        QJsonObject notificationJson;
        if (!StrictJson::readObject(json, QLatin1String("notification"), notificationJson))
        {
            return std::nullopt;
        }
        task.m_notification = TaskNotification::fromJson(notificationJson);
        if (!task.m_notification.has_value())
        {
            return std::nullopt;
        }
    }

    if (json.contains("resolvedAt"))
    {
        QDateTime resolvedAt;
        if (!StrictJson::readTime(json, QLatin1String("resolvedAt"), resolvedAt))
        {
            return std::nullopt;
        }
        task.m_resolvedAt = resolvedAt;
    }

    return task;
}

bool ResponseTask::operator==(const ResponseTask& other) const
{
    return m_id == other.m_id
        && m_category == other.m_category
        && m_description == other.m_description
        && m_createdAt == other.m_createdAt
        && m_assignedTeamId == other.m_assignedTeamId
        && m_assignedPersonId == other.m_assignedPersonId
        && m_assignmentNotes == other.m_assignmentNotes
        && m_notification == other.m_notification
        && m_resolved == other.m_resolved
        && m_resolutionNotes == other.m_resolutionNotes
        && m_resolvedAt == other.m_resolvedAt;
}

// === FamilyResponseRecord ===

FamilyResponseRecord FamilyResponseRecord::create(const FamilyId& familyId,
                                                   const QString& displayName,
                                                   const QString& address)
{
    FamilyResponseRecord record;
    record.m_familyId = familyId;
    record.m_displayName = displayName;
    record.m_address = address;
    return record;
}

void FamilyResponseRecord::addContactAttempt(const ContactAttempt& attempt)
{
    m_contactAttempts.prepend(attempt);
}

void FamilyResponseRecord::removeContactAttempt(const ContactAttemptId& id)
{
    for (int i = 0; i < m_contactAttempts.size(); ++i)
    {
        if (m_contactAttempts[i].id() == id)
        {
            m_contactAttempts.removeAt(i);
            return;
        }
    }
}

void FamilyResponseRecord::addTask(const ResponseTask& task)
{
    m_tasks.prepend(task);
}

void FamilyResponseRecord::updateTask(const ResponseTask& task)
{
    for (int i = 0; i < m_tasks.size(); ++i)
    {
        if (m_tasks[i].id() == task.id())
        {
            m_tasks[i] = task;
            return;
        }
    }
}

ResponseTask* FamilyResponseRecord::mutableTask(const TaskId& id)
{
    for (int i = 0; i < m_tasks.size(); ++i)
    {
        if (m_tasks[i].id() == id)
        {
            return &m_tasks[i];
        }
    }
    return nullptr;
}

void FamilyResponseRecord::removeTask(const TaskId& id)
{
    for (int i = 0; i < m_tasks.size(); ++i)
    {
        if (m_tasks[i].id() == id)
        {
            m_tasks.removeAt(i);
            return;
        }
    }
}

bool FamilyResponseRecord::needsHelp() const
{
    return unresolvedTaskCount() > 0;
}

int FamilyResponseRecord::unresolvedTaskCount() const
{
    int count = 0;
    for (const ResponseTask& task : m_tasks)
    {
        if (!task.isResolved())
        {
            ++count;
        }
    }
    return count;
}

EffectiveContactStatus FamilyResponseRecord::effectiveStatus() const
{
    if (needsHelp())
    {
        return EffectiveContactStatus::NeedsHelp;
    }

    switch (m_contactStatus)
    {
    case ContactStatus::NotContacted:
        return EffectiveContactStatus::NotContacted;
    case ContactStatus::OK:
        return EffectiveContactStatus::OK;
    case ContactStatus::UnableToReach:
        return EffectiveContactStatus::UnableToReach;
    }
    return EffectiveContactStatus::NotContacted;
}

QJsonObject FamilyResponseRecord::toJson() const
{
    QJsonObject json;
    json["familyId"] = m_familyId.toString();
    json["displayName"] = m_displayName;
    if (!m_address.isEmpty())
    {
        json["address"] = m_address;
    }
    json["contactStatus"] = contactStatusToString(m_contactStatus);

    if (!m_contactAttempts.isEmpty())
    {
        QJsonArray attemptsArray;
        for (const ContactAttempt& attempt : m_contactAttempts)
        {
            attemptsArray.append(attempt.toJson());
        }
        json["contactAttempts"] = attemptsArray;
    }

    if (!m_tasks.isEmpty())
    {
        QJsonArray tasksArray;
        for (const ResponseTask& task : m_tasks)
        {
            tasksArray.append(task.toJson());
        }
        json["tasks"] = tasksArray;
    }

    return json;
}

std::optional<FamilyResponseRecord> FamilyResponseRecord::fromJson(const QJsonObject& json)
{
    FamilyResponseRecord record;
    QString status;
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("familyId"),
                                  QLatin1String("displayName"),
                                  QLatin1String("address"),
                                  QLatin1String("contactStatus"),
                                  QLatin1String("contactAttempts"),
                                  QLatin1String("tasks")})
        || !StrictJson::readId(json, QLatin1String("familyId"), record.m_familyId)
        || !StrictJson::readString(json, QLatin1String("displayName"), record.m_displayName)
        || !readNotes(json, QLatin1String("address"), record.m_address)
        || !StrictJson::readString(json, QLatin1String("contactStatus"), status))
    {
        return std::nullopt;
    }

    const std::optional<ContactStatus> contactStatus = contactStatusFromString(status);
    if (!contactStatus.has_value())
    {
        return std::nullopt;
    }
    record.m_contactStatus = contactStatus.value();

    // Appended in the order written, which is already most recent first.
    if (json.contains("contactAttempts"))
    {
        QList<QJsonObject> attempts;
        if (!StrictJson::readObjectArray(json, QLatin1String("contactAttempts"), attempts) || attempts.isEmpty())
        {
            return std::nullopt;
        }
        QSet<ContactAttemptId> seen;
        for (const QJsonObject& attemptJson : attempts)
        {
            const std::optional<ContactAttempt> attempt = ContactAttempt::fromJson(attemptJson);
            if (!attempt.has_value() || seen.contains(attempt.value().id()))
            {
                return std::nullopt;
            }
            seen.insert(attempt.value().id());
            record.m_contactAttempts.append(attempt.value());
        }
    }

    if (json.contains("tasks"))
    {
        QList<QJsonObject> tasks;
        if (!StrictJson::readObjectArray(json, QLatin1String("tasks"), tasks) || tasks.isEmpty())
        {
            return std::nullopt;
        }
        QSet<TaskId> seen;
        for (const QJsonObject& taskJson : tasks)
        {
            const std::optional<ResponseTask> task = ResponseTask::fromJson(taskJson);
            if (!task.has_value() || seen.contains(task.value().id()))
            {
                return std::nullopt;
            }
            seen.insert(task.value().id());
            record.m_tasks.append(task.value());
        }
    }

    return record;
}

bool FamilyResponseRecord::operator==(const FamilyResponseRecord& other) const
{
    return m_familyId == other.m_familyId
        && m_displayName == other.m_displayName
        && m_address == other.m_address
        && m_contactStatus == other.m_contactStatus
        && m_contactAttempts == other.m_contactAttempts
        && m_tasks == other.m_tasks;
}

// === EmergencyResponse ===

EmergencyResponse EmergencyResponse::create(const QString& name)
{
    EmergencyResponse response;
    response.m_name = name;
    response.m_startedAt = QDateTime::currentDateTimeUtc();
    response.m_taskCategories = {"Tree removal", "Flooding", "Generator", "Medical", "Transport", "Shelter", "Other"};
    return response;
}

void EmergencyResponse::addFamilyRecord(const FamilyResponseRecord& record)
{
    m_familyRecords.insert(record.familyId(), record);
}

FamilyResponseRecord* EmergencyResponse::mutableRecord(const FamilyId& familyId)
{
    auto it = m_familyRecords.find(familyId);
    if (it != m_familyRecords.end())
    {
        return &it.value();
    }
    return nullptr;
}

const FamilyResponseRecord* EmergencyResponse::findRecord(const FamilyId& familyId) const
{
    auto it = m_familyRecords.find(familyId);
    if (it != m_familyRecords.end())
    {
        return &it.value();
    }
    return nullptr;
}

void EmergencyResponse::addTaskCategory(const QString& category)
{
    if (!m_taskCategories.contains(category))
    {
        m_taskCategories.append(category);
    }
}

int EmergencyResponse::totalFamilies() const
{
    return m_familyRecords.size();
}

int EmergencyResponse::countByStatus(EffectiveContactStatus status) const
{
    int count = 0;
    for (auto it = m_familyRecords.constBegin(); it != m_familyRecords.constEnd(); ++it)
    {
        if (it.value().effectiveStatus() == status)
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
    json["startedAt"] = StrictJson::writeTime(m_startedAt);

    if (m_endedAt.has_value())
    {
        json["endedAt"] = StrictJson::writeTime(m_endedAt.value());
    }

    QJsonArray recordsArray;
    for (auto it = m_familyRecords.constBegin(); it != m_familyRecords.constEnd(); ++it)
    {
        recordsArray.append(it.value().toJson());
    }
    json["familyRecords"] = recordsArray;

    QJsonArray categoriesArray;
    for (const QString& category : m_taskCategories)
    {
        categoriesArray.append(category);
    }
    json["taskCategories"] = categoriesArray;

    return json;
}

std::optional<EmergencyResponse> EmergencyResponse::fromJson(const QJsonObject& json)
{
    EmergencyResponse response;
    QList<QJsonObject> records;
    if (!StrictJson::hasOnlyKeys(json,
                                 {QLatin1String("name"),
                                  QLatin1String("startedAt"),
                                  QLatin1String("endedAt"),
                                  QLatin1String("familyRecords"),
                                  QLatin1String("taskCategories")})
        || !StrictJson::readString(json, QLatin1String("name"), response.m_name)
        || !StrictJson::readTime(json, QLatin1String("startedAt"), response.m_startedAt)
        || !StrictJson::readObjectArray(json, QLatin1String("familyRecords"), records)
        || !StrictJson::readStringArray(json, QLatin1String("taskCategories"), response.m_taskCategories))
    {
        return std::nullopt;
    }

    // A set kept in the order categories were added; addTaskCategory never repeats one.
    if (response.m_taskCategories.removeDuplicates() != 0)
    {
        return std::nullopt;
    }

    if (json.contains("endedAt"))
    {
        QDateTime endedAt;
        if (!StrictJson::readTime(json, QLatin1String("endedAt"), endedAt))
        {
            return std::nullopt;
        }
        response.m_endedAt = endedAt;
    }

    for (const QJsonObject& recordJson : records)
    {
        const std::optional<FamilyResponseRecord> record = FamilyResponseRecord::fromJson(recordJson);
        if (!record.has_value() || response.m_familyRecords.contains(record.value().familyId()))
        {
            return std::nullopt;
        }
        response.m_familyRecords.insert(record.value().familyId(), record.value());
    }

    return response;
}

bool EmergencyResponse::operator==(const EmergencyResponse& other) const
{
    return m_name == other.m_name
        && m_startedAt == other.m_startedAt
        && m_endedAt == other.m_endedAt
        && m_familyRecords == other.m_familyRecords
        && m_taskCategories == other.m_taskCategories;
}
