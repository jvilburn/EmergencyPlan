#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QJsonObject>
#include <optional>

#include "Id.h"

// === Enums ===

enum class ContactStatus
{
    NotContacted,
    OK,
    UnableToReach
    // "Needs Help" is derived from having unresolved tasks - not stored
};

enum class EffectiveContactStatus
{
    NotContacted,
    OK,
    NeedsHelp,      // derived
    UnableToReach
};

enum class ContactMethod
{
    Phone,
    Text,
    Email,
    Visit,
    Other
};

// === TaskNotification ===

class TaskNotification
{
public:
    TaskNotification() = default;

    static TaskNotification create(ContactMethod method, const QString& notes);

    ContactMethod method() const { return m_method; }
    const QDateTime& timestamp() const { return m_timestamp; }
    const QString& notes() const { return m_notes; }

    QJsonObject toJson() const;
    static TaskNotification fromJson(const QJsonObject& json);
    bool operator==(const TaskNotification& other) const;
    bool operator!=(const TaskNotification& other) const { return !(*this == other); }

private:
    ContactMethod m_method = ContactMethod::Phone;
    QDateTime m_timestamp;
    QString m_notes;
};

// === ContactAttempt ===

class ContactAttempt
{
public:
    ContactAttempt() = default;

    static ContactAttempt create(ContactMethod method, const PersonId& who, const QString& notes);

    const ContactAttemptId& id() const { return m_id; }
    ContactMethod method() const { return m_method; }
    const PersonId& who() const { return m_who; }
    const QDateTime& timestamp() const { return m_timestamp; }
    const QString& notes() const { return m_notes; }

    void setMethod(ContactMethod method) { m_method = method; }
    void setNotes(const QString& notes) { m_notes = notes; }

    QJsonObject toJson() const;
    static ContactAttempt fromJson(const QJsonObject& json);
    bool operator==(const ContactAttempt& other) const;
    bool operator!=(const ContactAttempt& other) const { return !(*this == other); }

private:
    ContactAttemptId m_id;
    ContactMethod m_method = ContactMethod::Phone;
    PersonId m_who;
    QDateTime m_timestamp;
    QString m_notes;
};

// === ResponseTask ===

class ResponseTask
{
public:
    ResponseTask() = default;

    static ResponseTask create(const QString& category, const QString& description);

    const TaskId& id() const { return m_id; }
    const QString& category() const { return m_category; }
    const QString& description() const { return m_description; }
    const QDateTime& createdAt() const { return m_createdAt; }

    // Assignment - at most one set
    const std::optional<TeamId>& assignedTeamId() const { return m_assignedTeamId; }
    const std::optional<PersonId>& assignedPersonId() const { return m_assignedPersonId; }
    const QString& assignmentNotes() const { return m_assignmentNotes; }
    const std::optional<TaskNotification>& notification() const { return m_notification; }

    void assignToTeam(const TeamId& teamId, const QString& notes);
    void assignToPerson(const PersonId& personId, const QString& notes);
    void clearAssignment();
    void setNotification(const TaskNotification& notification);

    // Resolution
    bool isResolved() const { return m_resolved; }
    const QString& resolutionNotes() const { return m_resolutionNotes; }
    const std::optional<QDateTime>& resolvedAt() const { return m_resolvedAt; }

    void resolve(const QString& notes);
    void unresolve();

    bool isAssigned() const;
    bool isNotified() const;

    QJsonObject toJson() const;
    static ResponseTask fromJson(const QJsonObject& json);
    bool operator==(const ResponseTask& other) const;
    bool operator!=(const ResponseTask& other) const { return !(*this == other); }

private:
    TaskId m_id;
    QString m_category;
    QString m_description;
    QDateTime m_createdAt;

    std::optional<TeamId> m_assignedTeamId;
    std::optional<PersonId> m_assignedPersonId;
    QString m_assignmentNotes;
    std::optional<TaskNotification> m_notification;

    bool m_resolved = false;
    QString m_resolutionNotes;
    std::optional<QDateTime> m_resolvedAt;
};

// === FamilyResponseRecord ===

class FamilyResponseRecord
{
public:
    FamilyResponseRecord() = default;

    static FamilyResponseRecord create(const FamilyId& familyId,
                                       const QString& displayName,
                                       const QString& address);

    const FamilyId& familyId() const { return m_familyId; }
    const QString& displayName() const { return m_displayName; }
    const QString& address() const { return m_address; }
    ContactStatus contactStatus() const { return m_contactStatus; }
    const QList<ContactAttempt>& contactAttempts() const { return m_contactAttempts; }
    const QList<ResponseTask>& tasks() const { return m_tasks; }

    void setContactStatus(ContactStatus status) { m_contactStatus = status; }
    void addContactAttempt(const ContactAttempt& attempt);
    void removeContactAttempt(const ContactAttemptId& id);
    void addTask(const ResponseTask& task);
    void updateTask(const ResponseTask& task);
    void removeTask(const TaskId& id);
    ResponseTask* mutableTask(const TaskId& id);

    // Derived status: NeedsHelp when contacted but has unresolved tasks
    bool needsHelp() const;
    int unresolvedTaskCount() const;

    // Effective status for display (combines stored + derived)
    EffectiveContactStatus effectiveStatus() const;

    QJsonObject toJson() const;
    static FamilyResponseRecord fromJson(const QJsonObject& json);
    bool operator==(const FamilyResponseRecord& other) const;
    bool operator!=(const FamilyResponseRecord& other) const { return !(*this == other); }

private:
    FamilyId m_familyId;
    QString m_displayName;       // snapshot at emergency start, not updated
    QString m_address;           // snapshot at emergency start, not updated
    ContactStatus m_contactStatus = ContactStatus::NotContacted;
    QList<ContactAttempt> m_contactAttempts;  // most recent first
    QList<ResponseTask> m_tasks;             // most recent first
};

// === EmergencyResponse ===

class EmergencyResponse
{
public:
    EmergencyResponse() = default;

    static EmergencyResponse create(const QString& name);

    const QString& name() const { return m_name; }
    const QDateTime& startedAt() const { return m_startedAt; }
    const std::optional<QDateTime>& endedAt() const { return m_endedAt; }
    const QHash<FamilyId, FamilyResponseRecord>& familyRecords() const { return m_familyRecords; }
    const QStringList& taskCategories() const { return m_taskCategories; }

    void setEndedAt(const QDateTime& endedAt) { m_endedAt = endedAt; }

    // Family record management (called by EmergencyManager)
    void addFamilyRecord(const FamilyResponseRecord& record);
    FamilyResponseRecord* mutableRecord(const FamilyId& familyId);
    const FamilyResponseRecord* findRecord(const FamilyId& familyId) const;

    // Task categories
    void addTaskCategory(const QString& category);

    // Statistics
    int totalFamilies() const;
    int countByStatus(EffectiveContactStatus status) const;

    QJsonObject toJson() const;
    static EmergencyResponse fromJson(const QJsonObject& json);
    bool operator==(const EmergencyResponse& other) const;
    bool operator!=(const EmergencyResponse& other) const { return !(*this == other); }

private:
    QString m_name;
    QDateTime m_startedAt;
    std::optional<QDateTime> m_endedAt;          // set when archived
    QHash<FamilyId, FamilyResponseRecord> m_familyRecords;
    QStringList m_taskCategories;
};
