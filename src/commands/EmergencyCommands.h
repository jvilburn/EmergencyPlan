#pragma once

#include "Command.h"
#include "EmergencyResponse.h"
#include "FieldChange.h"

#include <optional>

/// A new emergency response, an addition that never conflicts: its result holds when a
/// response with its id is already there. Undoing a start would delete a response other
/// responders may already be writing to, so it is not undoable.
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

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyResponse m_response;
};

/// An emergency ended, holding the response as its report was written from: it conflicts
/// when the response differs from that, so the report of the end that removes a response
/// holds everything in it, and its result holds when the response is already gone.
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

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyResponse m_response;
};

/// The inverse of EndEmergencyCommand: adds the response back as it was removed. Its result
/// holds when a response with that id is present.
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

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyResponse m_response;
};

/// A contact attempt added to a response, from the contact dialog or the OK or Unable to
/// Reach button: an addition. Its result holds when the response holds the attempt's id;
/// it conflicts when the response is gone. The attempt keeps the family and who it was made
/// with, with no condition that they exist, since the attempts are the log of what
/// happened.
class AddContactAttemptCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "addContactAttempt";

    /// The command that adds attempt, naming its family and who made it as document holds
    /// them.
    static UndoableCommandPtr fromNew(const EmergencyResponseId& responseId, const ContactAttempt& attempt, const Document& document);

    AddContactAttemptCommand(const EmergencyResponseId& responseId, const ContactAttempt& attempt, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyResponseId m_responseId;
    ContactAttempt m_attempt;
};

/// The inverse of AddContactAttemptCommand, which is how an attempt is undone. It removes
/// the attempt by id: its result holds when the attempt is already gone, and it conflicts
/// when the response is gone. It holds the whole attempt for its inverse to add back.
class DeleteContactAttemptCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "deleteContactAttempt";

    DeleteContactAttemptCommand(const EmergencyResponseId& responseId, const ContactAttempt& attempt, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyResponseId m_responseId;
    ContactAttempt m_attempt;
};

/// A new task in a response: an addition. Its result holds when the response holds the
/// task's id; it conflicts when the response is gone, or when the task is assigned to a team
/// or person the document lacks.
class AddTaskCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "addTask";

    /// The command that adds task, naming its family and assignee as document holds them.
    static UndoableCommandPtr fromNew(const EmergencyResponseId& responseId, const ResponseTask& task, const Document& document);

    AddTaskCommand(const EmergencyResponseId& responseId, const ResponseTask& task, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyResponseId m_responseId;
    ResponseTask m_task;
};

/// An edit to one task, built from the difference between the task as opened and as saved:
/// its category, description, assignment and resolution, each judged on its own. It
/// conflicts when the response or task is gone, a value moved, or the assignment it sets
/// names a team or person the document lacks. It holds the task's family for its document
/// change.
class UpdateTaskCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "updateTask";

    /// The edit from the task as opened to the task as saved, naming its family and
    /// assignee as document holds them, or an empty pointer when nothing differs, which
    /// WardSession::commit takes as nothing to do.
    static UndoableCommandPtr fromEdit(const EmergencyResponseId& responseId,
                                       const ResponseTask& opened,
                                       const ResponseTask& saved,
                                       const Document& document);

    UpdateTaskCommand(const EmergencyResponseId& responseId,
                      const FamilyId& familyId,
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

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyResponseId m_responseId;
    FamilyId m_familyId;
    TaskId m_taskId;
    std::optional<FieldChange<QString>> m_category;
    std::optional<FieldChange<QString>> m_taskDescription;
    std::optional<FieldChange<std::optional<TaskAssignment>>> m_assignment;
    std::optional<FieldChange<std::optional<TaskResolution>>> m_resolution;
};

/// A task removed, held as last seen: a change to the task since makes the removal
/// conflict, and the task being gone already means its result holds. It conflicts when the
/// response is gone.
class DeleteTaskCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "deleteTask";

    /// The command that removes task as the user sees it, naming its family and assignee as
    /// document holds them.
    static UndoableCommandPtr fromSeen(const EmergencyResponseId& responseId, const ResponseTask& task, const Document& document);

    DeleteTaskCommand(const EmergencyResponseId& responseId, const ResponseTask& task, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyResponseId m_responseId;
    ResponseTask m_task;
};

/// A task category added to a response. Its result holds when the response has the
/// category; it conflicts when the response is gone. An unused category does no harm, so
/// it is not undoable.
class AddTaskCategoryCommand : public Command
{
public:
    static constexpr const char* TypeName = "addTaskCategory";

    static CommandPtr fromNew(const EmergencyResponseId& responseId, const QString& category);

    AddTaskCategoryCommand(const EmergencyResponseId& responseId, const QString& category, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyResponseId m_responseId;
    QString m_category;
};
