#include "EmergencyResponse.h"

// === TaskNotification ===

TaskNotification TaskNotification::create(ContactMethod method, const QString& notes)
{
    TaskNotification n;
    n.m_method = method;
    n.m_timestamp = QDateTime::currentDateTimeUtc();
    n.m_notes = notes;
    return n;
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

void ResponseTask::reopen()
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
