#pragma once

#include "Command.h"
#include "EmergencyResponse.h"
#include "FieldChange.h"

#include <optional>

/// A new emergency, an addition that never conflicts: its result holds when an emergency is
/// already in progress, so two responders who each started one land in one emergency.
/// Undoing a start would delete an emergency other responders may already be writing to,
/// so it is not undoable.
class StartEmergencyCommand : public Command
{
public:
    static constexpr const char* TypeName = "startEmergency";

    static CommandPtr fromNew(const EmergencyResponse& response);

    StartEmergencyCommand(const EmergencyResponse& response, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    EmergencyResponse m_response;
};

/// The emergency ended, holding it as its report was written from: it conflicts when the
/// emergency in progress differs from that, so the report of the end that removes it holds
/// everything in it, and its result holds when none is in progress.
class EndEmergencyCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "endEmergency";

    /// The command that ends response, as the report was written from it.
    static UndoableCommandPtr fromReported(const EmergencyResponse& response);

    EndEmergencyCommand(const EmergencyResponse& response, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    EmergencyResponse m_response;
};

/// The inverse of EndEmergencyCommand: puts the emergency back as it was removed. Its result
/// holds when the emergency in progress equals it, and it conflicts when a different one is
/// in progress.
class RestoreEmergencyCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "restoreEmergency";

    RestoreEmergencyCommand(const EmergencyResponse& response, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    EmergencyResponse m_response;
};

/// A contact attempt added to the emergency in progress, from the contact dialog or the OK
/// or Unable to Reach button: an addition. Its result holds when the emergency holds the
/// attempt's id; it conflicts when none is in progress. The attempt keeps the family and who
/// it was made with, by id and by name, with no condition that they exist, since the
/// attempts are the log of what happened.
class AddContactAttemptCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "addContactAttempt";

    /// The command that adds attempt, naming its family and who made it as the attempt
    /// recorded them.
    static UndoableCommandPtr fromNew(const ContactAttempt& attempt);

    AddContactAttemptCommand(const ContactAttempt& attempt, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    ContactAttempt m_attempt;
};

/// The inverse of AddContactAttemptCommand, which is how an attempt is undone. It removes
/// the attempt by id: its result holds when the attempt is already gone, and it conflicts
/// when no emergency is in progress. It holds the whole attempt for its inverse to add back.
class DeleteContactAttemptCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "deleteContactAttempt";

    DeleteContactAttemptCommand(const ContactAttempt& attempt, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    ContactAttempt m_attempt;
};

/// A new task in the emergency in progress: an addition. Its result holds when the emergency
/// holds the task's id; it conflicts when none is in progress, or when the task is open and
/// assigned to a team or person the document lacks. A resolved task's assignee is part of
/// the log, so it takes no such condition.
class AddTaskCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "addTask";

    /// The command that adds task, naming its family and assignee as the task recorded them.
    static UndoableCommandPtr fromNew(const ResponseTask& task);

    AddTaskCommand(const ResponseTask& task, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    ResponseTask m_task;
};

/// An edit to one task, built from the difference between the task as opened and as saved:
/// its category, description, assignment and resolution, each judged on its own. It
/// conflicts when no emergency is in progress, the task is gone, a value moved, or the task
/// it leaves open is assigned to a team or person the document lacks.
/// It holds the task's family for its document change.
class UpdateTaskCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "updateTask";

    /// The edit from the task as opened to the task as saved, naming its family and
    /// assignee as the tasks recorded them, or an empty pointer when nothing differs, which
    /// WardSession::commit takes as nothing to do.
    static UndoableCommandPtr fromEdit(const ResponseTask& opened, const ResponseTask& saved);

    /// The edit that reopens task, unassigning it when document no longer holds its
    /// assignee, since an open task names only someone who can do it.
    static UndoableCommandPtr reopening(const ResponseTask& task, const Document& document);

    UpdateTaskCommand(const FamilyId& familyId,
                      const TaskId& taskId,
                      const std::optional<FieldChange<QString>>& category,
                      const std::optional<FieldChange<QString>>& taskDescription,
                      const std::optional<FieldChange<std::optional<TaskAssignment>>>& assignment,
                      const std::optional<FieldChange<std::optional<TaskResolution>>>& resolution,
                      const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    FamilyId m_familyId;
    TaskId m_taskId;
    std::optional<FieldChange<QString>> m_category;
    std::optional<FieldChange<QString>> m_taskDescription;
    std::optional<FieldChange<std::optional<TaskAssignment>>> m_assignment;
    std::optional<FieldChange<std::optional<TaskResolution>>> m_resolution;
};

/// A task removed, held as last seen: a change to the task since makes the removal
/// conflict, and the task being gone already means its result holds. It conflicts when no
/// emergency is in progress.
class DeleteTaskCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "deleteTask";

    /// The command that removes task as the user sees it, naming its family and assignee as
    /// the task recorded them.
    static UndoableCommandPtr fromSeen(const ResponseTask& task);

    DeleteTaskCommand(const ResponseTask& task, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    ResponseTask m_task;
};

/// A task category added to the emergency in progress. Its result holds when the emergency
/// has the category; it conflicts when none is in progress. An unused category does no
/// harm, so it is not undoable.
class AddTaskCategoryCommand : public Command
{
public:
    static constexpr const char* TypeName = "addTaskCategory";

    static CommandPtr fromNew(const QString& category);

    AddTaskCategoryCommand(const QString& category, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QString m_category;
};
