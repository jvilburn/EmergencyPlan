#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

#include "Family.h"
#include "Id.h"

// === Enums ===

enum class EffectiveContactStatus
{
    NotContacted,
    OK,
    NeedsHelp,
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

/// What a contact attempt found, when it says.
enum class ContactOutcome
{
    OK,
    UnableToReach
};

/// The method's name as the user sees it: "Phone", "Text", "Email", "Visit" or "Other".
QString contactMethodName(ContactMethod method);

/// The outcome's name as the user sees it: "OK" or "Unable to Reach".
QString contactOutcomeName(ContactOutcome outcome);

// === TaskNotification ===

/// The assignee was told about a task: how, when, and what was said.
class TaskNotification
{
public:
    TaskNotification() = default;

    /// A notification made now.
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

/// One try at reaching a family. The OK and Unable to Reach buttons record only an outcome;
/// the contact dialog records a method and who made the contact.
class ContactAttempt
{
public:
    ContactAttempt() = default;

    /// An attempt made now, with a fresh id.
    static ContactAttempt create(std::optional<ContactMethod> method,
                                 const std::optional<PersonId>& who,
                                 std::optional<ContactOutcome> outcome,
                                 const QString& notes);

    const ContactAttemptId& id() const { return m_id; }
    std::optional<ContactMethod> method() const { return m_method; }
    const std::optional<PersonId>& who() const { return m_who; }
    std::optional<ContactOutcome> outcome() const { return m_outcome; }
    const QDateTime& timestamp() const { return m_timestamp; }
    const QString& notes() const { return m_notes; }

    QJsonObject toJson() const;
    static ContactAttempt fromJson(const QJsonObject& json);

    bool operator==(const ContactAttempt& other) const;
    bool operator!=(const ContactAttempt& other) const { return !(*this == other); }

private:
    ContactAttemptId m_id;
    std::optional<ContactMethod> m_method;
    std::optional<PersonId> m_who;
    std::optional<ContactOutcome> m_outcome;
    QDateTime m_timestamp;
    QString m_notes;
};

// === TaskAssignment ===

/// Who a task is given to - one team or one person - with notes for them, and whether they
/// have been told. A new assignment has no notification, so giving a task to someone else
/// leaves it untold until it is notified again.
class TaskAssignment
{
public:
    TaskAssignment() = default;

    static TaskAssignment toTeam(const TeamId& teamId, const QString& notes);
    static TaskAssignment toPerson(const PersonId& personId, const QString& notes);

    /// Exactly one of teamId and personId is set.
    const std::optional<TeamId>& teamId() const { return m_teamId; }
    const std::optional<PersonId>& personId() const { return m_personId; }
    const QString& notes() const { return m_notes; }
    const std::optional<TaskNotification>& notification() const { return m_notification; }

    void setNotification(const TaskNotification& notification) { m_notification = notification; }

    QJsonObject toJson() const;
    static TaskAssignment fromJson(const QJsonObject& json);

    bool operator==(const TaskAssignment& other) const;
    bool operator!=(const TaskAssignment& other) const { return !(*this == other); }

private:
    std::optional<TeamId> m_teamId;
    std::optional<PersonId> m_personId;
    QString m_notes;
    std::optional<TaskNotification> m_notification;
};

// === TaskResolution ===

/// A task marked done: when, and what was done.
class TaskResolution
{
public:
    TaskResolution() = default;

    /// A resolution made now.
    static TaskResolution create(const QString& notes);

    const QString& notes() const { return m_notes; }
    const QDateTime& resolvedAt() const { return m_resolvedAt; }

    QJsonObject toJson() const;
    static TaskResolution fromJson(const QJsonObject& json);

    bool operator==(const TaskResolution& other) const;
    bool operator!=(const TaskResolution& other) const { return !(*this == other); }

private:
    QString m_notes;
    QDateTime m_resolvedAt;
};

// === ResponseTask ===

/// Something a family needs done. It is resolved while it holds a resolution, and assigned
/// while it holds an assignment.
class ResponseTask
{
public:
    ResponseTask() = default;

    /// A task created now, with a fresh id, unassigned and unresolved.
    static ResponseTask create(const QString& category, const QString& description);

    const TaskId& id() const { return m_id; }
    const QString& category() const { return m_category; }
    const QString& description() const { return m_description; }
    const QDateTime& createdAt() const { return m_createdAt; }
    const std::optional<TaskAssignment>& assignment() const { return m_assignment; }
    const std::optional<TaskResolution>& resolution() const { return m_resolution; }

    void setCategory(const QString& category) { m_category = category; }
    void setDescription(const QString& description) { m_description = description; }
    void setAssignment(const std::optional<TaskAssignment>& assignment) { m_assignment = assignment; }
    void setResolution(const std::optional<TaskResolution>& resolution) { m_resolution = resolution; }

    bool isAssigned() const { return m_assignment.has_value(); }
    bool isNotified() const { return m_assignment.has_value() && m_assignment->notification().has_value(); }
    bool isResolved() const { return m_resolution.has_value(); }

    QJsonObject toJson() const;
    static ResponseTask fromJson(const QJsonObject& json);

    bool operator==(const ResponseTask& other) const;
    bool operator!=(const ResponseTask& other) const { return !(*this == other); }

private:
    TaskId m_id;
    QString m_category;
    QString m_description;
    QDateTime m_createdAt;
    std::optional<TaskAssignment> m_assignment;
    std::optional<TaskResolution> m_resolution;
};

// === FamilyResponseRecord ===

/// One family's log in an emergency response: its attempts and tasks, with the family's
/// name and address as they stood when the record was made, so it reads the same after the
/// family changes or is removed.
///
/// Attempts are held most recent first: greatest timestamp first, ties broken by the
/// greater id first. Tasks are held the same way by when they were created. So the same
/// attempts and tasks make an equal record whatever order they were inserted in.
class FamilyResponseRecord
{
public:
    FamilyResponseRecord() = default;

    /// A record with no attempts or tasks.
    static FamilyResponseRecord create(const FamilyId& familyId,
                                       const QString& displayName,
                                       const QString& address);

    const FamilyId& familyId() const { return m_familyId; }
    const QString& displayName() const { return m_displayName; }
    const QString& address() const { return m_address; }
    const QList<ContactAttempt>& contactAttempts() const { return m_contactAttempts; }
    const QList<ResponseTask>& tasks() const { return m_tasks; }

    /// The attempt or task with that id, or null when the record has none.
    const ContactAttempt* findContactAttempt(const ContactAttemptId& id) const;
    const ResponseTask* findTask(const TaskId& id) const;

    /// Inserts at its place in the order. The record holds no attempt or task with its id.
    void insertContactAttempt(const ContactAttempt& attempt);
    void insertTask(const ResponseTask& task);

    /// Replaces the task with task's id, which keeps its place since its creation time is
    /// unchanged.
    void setTask(const ResponseTask& task);

    void removeContactAttempt(const ContactAttemptId& id);
    void removeTask(const TaskId& id);

    /// Needs Help while a task is unresolved; otherwise the outcome of the most recent
    /// attempt that has one; otherwise Not Contacted.
    EffectiveContactStatus effectiveStatus() const;

    QJsonObject toJson() const;
    static FamilyResponseRecord fromJson(const QJsonObject& json);

    bool operator==(const FamilyResponseRecord& other) const;
    bool operator!=(const FamilyResponseRecord& other) const { return !(*this == other); }

private:
    FamilyId m_familyId;
    QString m_displayName;
    QString m_address;
    QList<ContactAttempt> m_contactAttempts;
    QList<ResponseTask> m_tasks;
};

// === EmergencyResponse ===

/// One emergency in progress. A family has a record once a command writes to it; a
/// directory family without one is Not Contacted.
class EmergencyResponse
{
public:
    EmergencyResponse() = default;

    /// A response started now, with a fresh id, the default task categories, and no family
    /// records.
    static EmergencyResponse create(const QString& name);

    const EmergencyResponseId& id() const { return m_id; }
    const QString& name() const { return m_name; }
    const QDateTime& startedAt() const { return m_startedAt; }
    const QHash<FamilyId, FamilyResponseRecord>& familyRecords() const { return m_familyRecords; }
    const QStringList& taskCategories() const { return m_taskCategories; }

    /// The record for that family, or null when the family has none.
    const FamilyResponseRecord* findRecord(const FamilyId& familyId) const;

    /// Sets the record for record's family to record, adding it when there is none.
    void setRecord(const FamilyResponseRecord& record);

    /// Appends category. The response does not hold it already.
    void addTaskCategory(const QString& category);

    /// The response's records, plus every family in families without one.
    int totalFamilies(const QHash<FamilyId, Family>& families) const;

    /// The records with that status, plus, for Not Contacted, every family in families
    /// without a record.
    int countByStatus(EffectiveContactStatus status, const QHash<FamilyId, Family>& families) const;

    QJsonObject toJson() const;
    static EmergencyResponse fromJson(const QJsonObject& json);

    bool operator==(const EmergencyResponse& other) const;
    bool operator!=(const EmergencyResponse& other) const { return !(*this == other); }

private:
    EmergencyResponseId m_id;
    QString m_name;
    QDateTime m_startedAt;
    QHash<FamilyId, FamilyResponseRecord> m_familyRecords;
    QStringList m_taskCategories;
};
