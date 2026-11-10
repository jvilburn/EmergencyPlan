#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <optional>

#include "Family.h"
#include "Id.h"
#include "JsonReader.h"
#include "Person.h"
#include "Team.h"

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

/// How an attempt with an outcome and no method, made by a button, reads: "Marked OK" or
/// "Marked Unable to Reach".
QString contactMarkName(ContactOutcome outcome);

class ContactAttempt;

/// The attempt's parts in the order every view shows them: time, method, who, outcome and
/// notes, leaving out the ones it lacks. The outcome of an attempt with no method reads as
/// its mark, and who is the name the attempt recorded. timeText is the time as the caller
/// writes it.
QStringList contactAttemptParts(const ContactAttempt& attempt, const QString& timeText);

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
    static TaskNotification fromJson(JsonReader json);

    bool operator==(const TaskNotification& other) const;
    bool operator!=(const TaskNotification& other) const { return !(*this == other); }

private:
    ContactMethod m_method = ContactMethod::Phone;
    QDateTime m_timestamp;
    QString m_notes;
};

// === ContactAttempt ===

/// One try at reaching a family. The OK and Unable to Reach buttons give only an outcome;
/// the contact dialog gives a method and who made the contact. It names the family and the
/// person by id and by name as they stood when it was made.
class ContactAttempt
{
public:
    ContactAttempt() = default;

    /// An attempt on family made now, with a fresh id, recording the family's and who's
    /// names as they stand.
    static ContactAttempt create(const Family& family,
                                 std::optional<ContactMethod> method,
                                 const std::optional<Person>& who,
                                 std::optional<ContactOutcome> outcome,
                                 const QString& notes);

    const ContactAttemptId& id() const { return m_id; }
    const FamilyId& familyId() const { return m_familyId; }
    const QString& familyName() const { return m_familyName; }
    std::optional<ContactMethod> method() const { return m_method; }
    const std::optional<PersonId>& who() const { return m_who; }

    /// Who's name as it stood when the attempt was made; empty when it names no one.
    const QString& whoName() const { return m_whoName; }
    std::optional<ContactOutcome> outcome() const { return m_outcome; }
    const QDateTime& timestamp() const { return m_timestamp; }
    const QString& notes() const { return m_notes; }

    QJsonObject toJson() const;
    static ContactAttempt fromJson(JsonReader json);

    bool operator==(const ContactAttempt& other) const;
    bool operator!=(const ContactAttempt& other) const { return !(*this == other); }

private:
    ContactAttemptId m_id;
    FamilyId m_familyId;
    QString m_familyName;
    std::optional<ContactMethod> m_method;
    std::optional<PersonId> m_who;
    QString m_whoName;
    std::optional<ContactOutcome> m_outcome;
    QDateTime m_timestamp;
    QString m_notes;
};

// === TaskAssignment ===

/// Who a task is given to - one team or one person - and whether they have been told. It
/// names the assignee by id and by name as they stood when the task was given. A new
/// assignment has no notification, so giving a task to someone else leaves it untold until
/// it is notified again.
class TaskAssignment
{
public:
    TaskAssignment() = default;

    static TaskAssignment toTeam(const Team& team);
    static TaskAssignment toPerson(const Person& person);

    /// Exactly one of teamId and personId is set.
    const std::optional<TeamId>& teamId() const { return m_teamId; }
    const std::optional<PersonId>& personId() const { return m_personId; }
    const QString& assigneeName() const { return m_assigneeName; }
    const std::optional<TaskNotification>& notification() const { return m_notification; }

    void setNotification(const TaskNotification& notification) { m_notification = notification; }

    QJsonObject toJson() const;
    static TaskAssignment fromJson(JsonReader json);

    bool operator==(const TaskAssignment& other) const;
    bool operator!=(const TaskAssignment& other) const { return !(*this == other); }

private:
    std::optional<TeamId> m_teamId;
    std::optional<PersonId> m_personId;
    QString m_assigneeName;
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
    static TaskResolution fromJson(JsonReader json);

    bool operator==(const TaskResolution& other) const;
    bool operator!=(const TaskResolution& other) const { return !(*this == other); }

private:
    QString m_notes;
    QDateTime m_resolvedAt;
};

// === ResponseTask ===

/// Something a family needs done. It is resolved while it holds a resolution, and assigned
/// while it holds an assignment. It names its family by id and by name as it stood when the
/// task was created.
class ResponseTask
{
public:
    ResponseTask() = default;

    /// A task for family created now, with a fresh id, unassigned and unresolved, recording
    /// the family's name as it stands.
    static ResponseTask create(const Family& family, const QString& category, const QString& description);

    const TaskId& id() const { return m_id; }
    const FamilyId& familyId() const { return m_familyId; }
    const QString& familyName() const { return m_familyName; }
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
    static ResponseTask fromJson(JsonReader json);

    bool operator==(const ResponseTask& other) const;
    bool operator!=(const ResponseTask& other) const { return !(*this == other); }

private:
    TaskId m_id;
    FamilyId m_familyId;
    QString m_familyName;
    QString m_category;
    QString m_description;
    QDateTime m_createdAt;
    std::optional<TaskAssignment> m_assignment;
    std::optional<TaskResolution> m_resolution;
};

// === EmergencyResponse ===

/// The emergency in progress: its contact attempts and tasks, each carrying the family it is
/// for. A family's status is worked out from the attempts and tasks that carry its id.
///
/// Attempts are held most recent first: greatest timestamp first, ties broken by the
/// greater id first. Tasks are held the same way by when they were created. So the same
/// attempts and tasks make an equal response whatever order they were inserted in.
class EmergencyResponse
{
public:
    EmergencyResponse() = default;

    /// A response started now, with the default task categories, and no attempts or tasks.
    static EmergencyResponse create(const QString& name);

    const QString& name() const { return m_name; }
    const QDateTime& startedAt() const { return m_startedAt; }
    const QStringList& taskCategories() const { return m_taskCategories; }
    const QList<ContactAttempt>& contactAttempts() const { return m_contactAttempts; }
    const QList<ResponseTask>& tasks() const { return m_tasks; }

    /// The attempt or task with that id, or null when the response has none.
    const ContactAttempt* findContactAttempt(const ContactAttemptId& id) const;
    const ResponseTask* findTask(const TaskId& id) const;

    /// Inserts at its place in the order. The response holds no attempt or task with its id.
    void insertContactAttempt(const ContactAttempt& attempt);
    void insertTask(const ResponseTask& task);

    /// Replaces the task with task's id, which keeps its place since its creation time is
    /// unchanged.
    void setTask(const ResponseTask& task);

    void removeContactAttempt(const ContactAttemptId& id);
    void removeTask(const TaskId& id);

    /// Appends category. The response does not hold it already.
    void addTaskCategory(const QString& category);

    /// Every family id that an attempt or task carries.
    QSet<FamilyId> familyIds() const;

    /// The family's attempts and tasks, in the response's order.
    QList<ContactAttempt> contactAttemptsFor(const FamilyId& familyId) const;
    QList<ResponseTask> tasksFor(const FamilyId& familyId) const;

    /// Each family's status, worked out in one pass: Needs Help while one of its tasks is
    /// unresolved, otherwise the outcome of its most recent attempt that has one. A family
    /// missing from it is Not Contacted.
    QHash<FamilyId, EffectiveContactStatus> statusByFamily() const;

    /// The families the statistics count: every family in families, and every family an
    /// attempt or task carries.
    QSet<FamilyId> countedFamilies(const QHash<FamilyId, Family>& families) const;

    /// The number of countedFamilies.
    int totalFamilies(const QHash<FamilyId, Family>& families) const;

    /// The families counted by totalFamilies whose status is status.
    int countByStatus(EffectiveContactStatus status, const QHash<FamilyId, Family>& families) const;

    QJsonObject toJson() const;
    static EmergencyResponse fromJson(JsonReader json);

    bool operator==(const EmergencyResponse& other) const;
    bool operator!=(const EmergencyResponse& other) const { return !(*this == other); }

private:
    QString m_name;
    QDateTime m_startedAt;
    QStringList m_taskCategories;
    QList<ContactAttempt> m_contactAttempts;
    QList<ResponseTask> m_tasks;
};
