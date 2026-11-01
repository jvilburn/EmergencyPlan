#pragma once

#include "Command.h"
#include "EmergencyResponse.h"
#include "FieldChange.h"

#include <optional>

/// Where a command writes in an emergency response: one family's record, with the family's
/// name and address as the command saw them. A command that writes to a family without a
/// record makes the record from these, so it depends on nothing but the response.
class FamilyRecordTarget
{
public:
    FamilyRecordTarget(const EmergencyResponseId& responseId,
                       const FamilyId& familyId,
                       const QString& displayName,
                       const QString& address);

    /// The target for family's record in that response, named and addressed as family is.
    static FamilyRecordTarget forFamily(const EmergencyResponseId& responseId, const Family& family);

    const EmergencyResponseId& responseId() const { return m_responseId; }
    const FamilyId& familyId() const { return m_familyId; }
    const QString& displayName() const { return m_displayName; }
    const QString& address() const { return m_address; }

    /// The response's record for the family, or a new record with this name and address
    /// when the response has none.
    FamilyResponseRecord recordIn(const EmergencyResponse& response) const;

    QJsonObject toJson() const;
    static FamilyRecordTarget fromJson(const QJsonObject& json);

private:
    EmergencyResponseId m_responseId;
    FamilyId m_familyId;
    QString m_displayName;
    QString m_address;
};

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

/// A contact attempt logged, or the OK or Unable to Reach button pressed, on one family: an
/// addition. Its result holds when the record holds the attempt's id; it conflicts when the
/// response is gone. The attempt keeps the who it was made with, with no condition that the
/// person exists, since the record is the log of what happened.
class RecordContactAttemptCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "recordContactAttempt";

    /// The command that records attempt, naming who made it as document holds them.
    static UndoableCommandPtr fromNew(const FamilyRecordTarget& target, const ContactAttempt& attempt, const Document& document);

    RecordContactAttemptCommand(const FamilyRecordTarget& target, const ContactAttempt& attempt, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    FamilyRecordTarget m_target;
    ContactAttempt m_attempt;
};

/// The inverse of RecordContactAttemptCommand, which is how an attempt is undone. The
/// attempt is held as last seen: its result holds when the attempt is already gone, and it
/// conflicts when the response is gone or the attempt differs.
class DeleteContactAttemptCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "deleteContactAttempt";

    DeleteContactAttemptCommand(const FamilyRecordTarget& target, const ContactAttempt& attempt, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    FamilyRecordTarget m_target;
    ContactAttempt m_attempt;
};

/// A new task for one family: an addition. Its result holds when the record holds the
/// task's id; it conflicts when the response is gone, or when the task is assigned to a team
/// or person the document lacks.
class AddTaskCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "addTask";

    /// The command that adds task, naming its assignee as document holds them.
    static UndoableCommandPtr fromNew(const FamilyRecordTarget& target, const ResponseTask& task, const Document& document);

    AddTaskCommand(const FamilyRecordTarget& target, const ResponseTask& task, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    FamilyRecordTarget m_target;
    ResponseTask m_task;
};

/// An edit to one task, built from the difference between the task as opened and as saved:
/// its category, description, assignment and resolution, each judged on its own. It
/// conflicts when the response, record or task is gone, a value moved, or the assignment it
/// leaves names a team or person the document lacks.
class UpdateTaskCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "updateTask";

    /// The edit from the task as opened to the task as saved, naming its assignee as
    /// document holds them, or an empty pointer when nothing differs, which
    /// WardSession::commit takes as nothing to do.
    static UndoableCommandPtr fromEdit(const FamilyRecordTarget& target,
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

    /// The command that removes task as the user sees it, naming its assignee as document
    /// holds them.
    static UndoableCommandPtr fromSeen(const FamilyRecordTarget& target, const ResponseTask& task, const Document& document);

    DeleteTaskCommand(const FamilyRecordTarget& target, const ResponseTask& task, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    FamilyRecordTarget m_target;
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
