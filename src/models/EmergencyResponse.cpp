#include "EmergencyResponse.h"

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

static ContactMethod contactMethodFromString(const QString& str)
{
    if (str == "text")
    {
        return ContactMethod::Text;
    }
    if (str == "email")
    {
        return ContactMethod::Email;
    }
    if (str == "visit")
    {
        return ContactMethod::Visit;
    }
    if (str == "other")
    {
        return ContactMethod::Other;
    }
    return ContactMethod::Phone;
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

static ContactStatus contactStatusFromString(const QString& str)
{
    if (str == "ok")
    {
        return ContactStatus::OK;
    }
    if (str == "unableToReach")
    {
        return ContactStatus::UnableToReach;
    }
    return ContactStatus::NotContacted;
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
    json["timestamp"] = m_timestamp.toString(Qt::ISODate);
    if (!m_notes.isEmpty())
    {
        json["notes"] = m_notes;
    }
    return json;
}

TaskNotification TaskNotification::fromJson(const QJsonObject& json)
{
    TaskNotification n;
    n.m_method = contactMethodFromString(json["method"].toString());
    n.m_timestamp = QDateTime::fromString(json["timestamp"].toString(), Qt::ISODate);
    n.m_notes = json["notes"].toString();
    return n;
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
    json["timestamp"] = m_timestamp.toString(Qt::ISODate);
    if (!m_notes.isEmpty())
    {
        json["notes"] = m_notes;
    }
    return json;
}

ContactAttempt ContactAttempt::fromJson(const QJsonObject& json)
{
    ContactAttempt attempt;
    attempt.m_id = ContactAttemptId::fromString(json["id"].toString());
    attempt.m_method = contactMethodFromString(json["method"].toString());
    attempt.m_who = PersonId::fromString(json["who"].toString());
    attempt.m_timestamp = QDateTime::fromString(json["timestamp"].toString(), Qt::ISODate);
    attempt.m_notes = json["notes"].toString();
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

void ResponseTask::setNotification(const TaskNotification& notification)
{
    m_notification = notification;
}

void ResponseTask::resolve(const QString& notes)
{
    m_resolved = true;
    m_resolutionNotes = notes;
    m_resolvedAt = QDateTime::currentDateTimeUtc();
}

void ResponseTask::unresolve()
{
    m_resolved = false;
    m_resolutionNotes.clear();
    m_resolvedAt.reset();
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
    json["createdAt"] = m_createdAt.toString(Qt::ISODate);

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
        json["resolvedAt"] = m_resolvedAt->toString(Qt::ISODate);
    }

    return json;
}

ResponseTask ResponseTask::fromJson(const QJsonObject& json)
{
    ResponseTask task;
    task.m_id = TaskId::fromString(json["id"].toString());
    task.m_category = json["category"].toString();
    task.m_description = json["description"].toString();
    task.m_createdAt = QDateTime::fromString(json["createdAt"].toString(), Qt::ISODate);

    QString teamIdStr = json["assignedTeamId"].toString();
    if (!teamIdStr.isEmpty())
    {
        task.m_assignedTeamId = TeamId::fromString(teamIdStr);
    }

    QString personIdStr = json["assignedPersonId"].toString();
    if (!personIdStr.isEmpty())
    {
        task.m_assignedPersonId = PersonId::fromString(personIdStr);
    }

    task.m_assignmentNotes = json["assignmentNotes"].toString();

    if (json.contains("notification"))
    {
        task.m_notification = TaskNotification::fromJson(json["notification"].toObject());
    }

    task.m_resolved = json["resolved"].toBool();
    task.m_resolutionNotes = json["resolutionNotes"].toString();

    QString resolvedAtStr = json["resolvedAt"].toString();
    if (!resolvedAtStr.isEmpty())
    {
        task.m_resolvedAt = QDateTime::fromString(resolvedAtStr, Qt::ISODate);
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
    return m_contactStatus != ContactStatus::NotContacted
        && unresolvedTaskCount() > 0;
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

FamilyResponseRecord FamilyResponseRecord::fromJson(const QJsonObject& json)
{
    FamilyResponseRecord record;
    record.m_familyId = FamilyId::fromString(json["familyId"].toString());
    record.m_displayName = json["displayName"].toString();
    record.m_address = json["address"].toString();
    record.m_contactStatus = contactStatusFromString(json["contactStatus"].toString());

    // append (not prepend) — JSON array is already in most-recent-first order from toJson()
    if (json.contains("contactAttempts"))
    {
        QJsonArray attemptsArray = json["contactAttempts"].toArray();
        for (const QJsonValue& value : attemptsArray)
        {
            record.m_contactAttempts.append(ContactAttempt::fromJson(value.toObject()));
        }
    }

    // append (not prepend) — JSON array is already in most-recent-first order from toJson()
    if (json.contains("tasks"))
    {
        QJsonArray tasksArray = json["tasks"].toArray();
        for (const QJsonValue& value : tasksArray)
        {
            record.m_tasks.append(ResponseTask::fromJson(value.toObject()));
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
    response.m_taskCategories = {"Tree removal", "Generator", "Medical", "Transport", "Shelter", "Other"};
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
    json["startedAt"] = m_startedAt.toString(Qt::ISODate);

    if (m_endedAt.has_value())
    {
        json["endedAt"] = m_endedAt->toString(Qt::ISODate);
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

EmergencyResponse EmergencyResponse::fromJson(const QJsonObject& json)
{
    EmergencyResponse response;
    response.m_name = json["name"].toString();
    response.m_startedAt = QDateTime::fromString(json["startedAt"].toString(), Qt::ISODate);

    QString endedAtStr = json["endedAt"].toString();
    if (!endedAtStr.isEmpty())
    {
        response.m_endedAt = QDateTime::fromString(endedAtStr, Qt::ISODate);
    }

    QJsonArray recordsArray = json["familyRecords"].toArray();
    for (const QJsonValue& value : recordsArray)
    {
        FamilyResponseRecord record = FamilyResponseRecord::fromJson(value.toObject());
        response.m_familyRecords.insert(record.familyId(), record);
    }

    QJsonArray categoriesArray = json["taskCategories"].toArray();
    for (const QJsonValue& value : categoriesArray)
    {
        response.m_taskCategories.append(value.toString());
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
