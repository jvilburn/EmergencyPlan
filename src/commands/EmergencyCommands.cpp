#include "EmergencyCommands.h"
#include "Document.h"
#include "FieldChangeJson.h"

#include <QObject>

namespace
{

QString orNone(const QString& text)
{
    return text.isEmpty() ? QObject::tr("(none)") : text;
}

/// The team's name as document holds it, or its id when document lacks it.
QString teamName(const TeamId& id, const Document& document)
{
    std::optional<Team> team = document.findTeamById(id);
    return team.has_value() ? team->name() : id.toString();
}

QString assigneeName(const TaskAssignment& assignment, const Document& document)
{
    if (assignment.teamId().has_value())
    {
        return teamName(*assignment.teamId(), document);
    }
    if (assignment.personId().has_value())
    {
        return personName(*assignment.personId(), document);
    }
    return QObject::tr("(none)");
}

/// True when there is no assignment, or document holds the team or person it names.
bool assigneeExists(const std::optional<TaskAssignment>& assignment, const Document& document)
{
    if (!assignment.has_value())
    {
        return true;
    }
    const std::optional<TeamId>& team = assignment->teamId();
    const std::optional<PersonId>& person = assignment->personId();
    return (team.has_value() && document.findTeamById(*team).has_value())
        || (person.has_value() && document.findPersonById(*person).has_value());
}

QStringList describeAttempt(const ContactAttempt& attempt, const Document& document)
{
    QStringList lines;
    if (attempt.method().has_value())
    {
        lines.append(QObject::tr("method: %1").arg(contactMethodName(*attempt.method())));
    }
    if (attempt.who().has_value())
    {
        lines.append(QObject::tr("who: %1").arg(personName(*attempt.who(), document)));
    }
    if (attempt.outcome().has_value())
    {
        lines.append(QObject::tr("outcome: %1").arg(contactOutcomeName(*attempt.outcome())));
    }
    if (!attempt.notes().isEmpty())
    {
        lines.append(QObject::tr("notes: %1").arg(attempt.notes()));
    }
    return lines;
}

QStringList describeAssignment(const std::optional<TaskAssignment>& assignment, const Document& document)
{
    if (!assignment.has_value())
    {
        return {QObject::tr("assigned to: (none)")};
    }
    QStringList lines;
    lines.append(QObject::tr("assigned to: %1").arg(assigneeName(*assignment, document)));
    if (!assignment->notes().isEmpty())
    {
        lines.append(QObject::tr("assignment notes: %1").arg(assignment->notes()));
    }
    const std::optional<TaskNotification>& notification = assignment->notification();
    if (notification.has_value())
    {
        QString method = contactMethodName(notification->method());
        if (notification->notes().isEmpty())
        {
            lines.append(QObject::tr("notified: %1").arg(method));
        }
        else
        {
            lines.append(QObject::tr("notified: %1, %2").arg(method, notification->notes()));
        }
    }
    return lines;
}

QString describeResolution(const std::optional<TaskResolution>& resolution)
{
    if (!resolution.has_value())
    {
        return QObject::tr("reopened");
    }
    return QObject::tr("resolved: %1").arg(orNone(resolution->notes()));
}

/// The task's description, and its assignment and resolution when it has them.
QStringList describeTask(const ResponseTask& task, const Document& document)
{
    QStringList lines;
    lines.append(QObject::tr("description: %1").arg(orNone(task.description())));
    if (task.assignment().has_value())
    {
        lines.append(describeAssignment(task.assignment(), document));
    }
    if (task.resolution().has_value())
    {
        lines.append(describeResolution(task.resolution()));
    }
    return lines;
}

/// Puts record into response, and response into document.
void writeRecord(Document& document, EmergencyResponse response, const FamilyResponseRecord& record)
{
    response.setRecord(record);
    document.setEmergencyResponse(response);
}

DocumentChange recordChange(const FamilyRecordTarget& target)
{
    return DocumentChange::emergencyResponse().updated(target.responseId(), target.familyId());
}

}  // namespace

// ----------------------------------------------------------------------------
// FamilyRecordTarget
// ----------------------------------------------------------------------------

FamilyRecordTarget::FamilyRecordTarget(const EmergencyResponseId& responseId,
                                       const FamilyId& familyId,
                                       const QString& displayName,
                                       const QString& address)
    : m_responseId(responseId)
    , m_familyId(familyId)
    , m_displayName(displayName)
    , m_address(address)
{
}

FamilyRecordTarget FamilyRecordTarget::forFamily(const EmergencyResponseId& responseId, const Family& family)
{
    return FamilyRecordTarget(responseId, family.id(), family.displayName(), family.address().full());
}

FamilyResponseRecord FamilyRecordTarget::recordIn(const EmergencyResponse& response) const
{
    const FamilyResponseRecord* record = response.findRecord(m_familyId);
    if (record)
    {
        return *record;
    }
    return FamilyResponseRecord::create(m_familyId, m_displayName, m_address);
}

QJsonObject FamilyRecordTarget::toJson() const
{
    QJsonObject json;
    json["responseId"] = m_responseId.toString();
    json["familyId"] = m_familyId.toString();
    json["displayName"] = m_displayName;
    json["address"] = m_address;
    return json;
}

FamilyRecordTarget FamilyRecordTarget::fromJson(const QJsonObject& json)
{
    return FamilyRecordTarget(EmergencyResponseId::fromString(json["responseId"].toString()),
                              FamilyId::fromString(json["familyId"].toString()),
                              json["displayName"].toString(),
                              json["address"].toString());
}

// ----------------------------------------------------------------------------
// StartEmergencyCommand
// ----------------------------------------------------------------------------

CommandPtr StartEmergencyCommand::fromNew(const EmergencyResponse& response)
{
    CommandDescription description(QObject::tr("Start emergency: %1").arg(response.name()), {});
    return std::make_shared<StartEmergencyCommand>(response, description);
}

StartEmergencyCommand::StartEmergencyCommand(const EmergencyResponse& response, const CommandDescription& description)
    : Command(description)
    , m_response(response)
{
}

CommandOutcome StartEmergencyCommand::execute(Document& document) const
{
    if (!document.findEmergencyResponseById(m_response.id()).has_value())
    {
        document.setEmergencyResponse(m_response);
    }
    return CommandOutcome::Applied;
}

QJsonObject StartEmergencyCommand::toJson() const
{
    QJsonObject json;
    json["response"] = m_response.toJson();
    return json;
}

DocumentChange StartEmergencyCommand::documentChange() const
{
    return DocumentChange::emergencyResponse().updated(m_response.id());
}

CommandPtr StartEmergencyCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<StartEmergencyCommand>(EmergencyResponse::fromJson(json["response"].toObject()), description);
}

// ----------------------------------------------------------------------------
// EndEmergencyCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr EndEmergencyCommand::fromReported(const EmergencyResponse& response)
{
    CommandDescription description(QObject::tr("End emergency: %1").arg(response.name()), {});
    return std::make_shared<EndEmergencyCommand>(response, description);
}

EndEmergencyCommand::EndEmergencyCommand(const EmergencyResponse& response, const CommandDescription& description)
    : UndoableCommand(description)
    , m_response(response)
{
}

CommandOutcome EndEmergencyCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.findEmergencyResponseById(m_response.id());
    if (!response.has_value())
    {
        return CommandOutcome::Applied;
    }
    if (*response != m_response)
    {
        return CommandOutcome::Conflicted;
    }
    document.removeEmergencyResponse(m_response.id());
    return CommandOutcome::Applied;
}

UndoableCommandPtr EndEmergencyCommand::inverse() const
{
    return std::make_shared<RestoreEmergencyCommand>(m_response, description().undone());
}

QJsonObject EndEmergencyCommand::toJson() const
{
    QJsonObject json;
    json["response"] = m_response.toJson();
    return json;
}

DocumentChange EndEmergencyCommand::documentChange() const
{
    return DocumentChange::emergencyResponse().updated(m_response.id());
}

CommandPtr EndEmergencyCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<EndEmergencyCommand>(EmergencyResponse::fromJson(json["response"].toObject()), description);
}

// ----------------------------------------------------------------------------
// RestoreEmergencyCommand
// ----------------------------------------------------------------------------

RestoreEmergencyCommand::RestoreEmergencyCommand(const EmergencyResponse& response, const CommandDescription& description)
    : UndoableCommand(description)
    , m_response(response)
{
}

CommandOutcome RestoreEmergencyCommand::execute(Document& document) const
{
    if (!document.findEmergencyResponseById(m_response.id()).has_value())
    {
        document.setEmergencyResponse(m_response);
    }
    return CommandOutcome::Applied;
}

UndoableCommandPtr RestoreEmergencyCommand::inverse() const
{
    return std::make_shared<EndEmergencyCommand>(m_response, description().undone());
}

QJsonObject RestoreEmergencyCommand::toJson() const
{
    QJsonObject json;
    json["response"] = m_response.toJson();
    return json;
}

DocumentChange RestoreEmergencyCommand::documentChange() const
{
    return DocumentChange::emergencyResponse().updated(m_response.id());
}

CommandPtr RestoreEmergencyCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<RestoreEmergencyCommand>(EmergencyResponse::fromJson(json["response"].toObject()), description);
}

// ----------------------------------------------------------------------------
// RecordContactAttemptCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr RecordContactAttemptCommand::fromNew(const FamilyRecordTarget& target,
                                                        const ContactAttempt& attempt,
                                                        const Document& document)
{
    CommandDescription description(QObject::tr("Contact with %1").arg(target.displayName()),
                                   describeAttempt(attempt, document));
    return std::make_shared<RecordContactAttemptCommand>(target, attempt, description);
}

RecordContactAttemptCommand::RecordContactAttemptCommand(const FamilyRecordTarget& target,
                                                         const ContactAttempt& attempt,
                                                         const CommandDescription& description)
    : UndoableCommand(description)
    , m_target(target)
    , m_attempt(attempt)
{
}

CommandOutcome RecordContactAttemptCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.findEmergencyResponseById(m_target.responseId());
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    FamilyResponseRecord record = m_target.recordIn(*response);
    if (record.findContactAttempt(m_attempt.id()))
    {
        return CommandOutcome::Applied;
    }
    record.insertContactAttempt(m_attempt);
    writeRecord(document, *response, record);
    return CommandOutcome::Applied;
}

UndoableCommandPtr RecordContactAttemptCommand::inverse() const
{
    return std::make_shared<DeleteContactAttemptCommand>(m_target, m_attempt, description().undone());
}

QJsonObject RecordContactAttemptCommand::toJson() const
{
    QJsonObject json;
    json["target"] = m_target.toJson();
    json["attempt"] = m_attempt.toJson();
    return json;
}

DocumentChange RecordContactAttemptCommand::documentChange() const
{
    return recordChange(m_target);
}

CommandPtr RecordContactAttemptCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<RecordContactAttemptCommand>(FamilyRecordTarget::fromJson(json["target"].toObject()),
                                                         ContactAttempt::fromJson(json["attempt"].toObject()),
                                                         description);
}

// ----------------------------------------------------------------------------
// DeleteContactAttemptCommand
// ----------------------------------------------------------------------------

DeleteContactAttemptCommand::DeleteContactAttemptCommand(const FamilyRecordTarget& target,
                                                         const ContactAttempt& attempt,
                                                         const CommandDescription& description)
    : UndoableCommand(description)
    , m_target(target)
    , m_attempt(attempt)
{
}

CommandOutcome DeleteContactAttemptCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.findEmergencyResponseById(m_target.responseId());
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    const FamilyResponseRecord* record = response->findRecord(m_target.familyId());
    const ContactAttempt* attempt = record ? record->findContactAttempt(m_attempt.id()) : nullptr;
    if (!attempt)
    {
        return CommandOutcome::Applied;
    }
    if (*attempt != m_attempt)
    {
        return CommandOutcome::Conflicted;
    }
    FamilyResponseRecord changed = *record;
    changed.removeContactAttempt(m_attempt.id());
    writeRecord(document, *response, changed);
    return CommandOutcome::Applied;
}

UndoableCommandPtr DeleteContactAttemptCommand::inverse() const
{
    return std::make_shared<RecordContactAttemptCommand>(m_target, m_attempt, description().undone());
}

QJsonObject DeleteContactAttemptCommand::toJson() const
{
    QJsonObject json;
    json["target"] = m_target.toJson();
    json["attempt"] = m_attempt.toJson();
    return json;
}

DocumentChange DeleteContactAttemptCommand::documentChange() const
{
    return recordChange(m_target);
}

CommandPtr DeleteContactAttemptCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<DeleteContactAttemptCommand>(FamilyRecordTarget::fromJson(json["target"].toObject()),
                                                         ContactAttempt::fromJson(json["attempt"].toObject()),
                                                         description);
}

// ----------------------------------------------------------------------------
// AddTaskCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr AddTaskCommand::fromNew(const FamilyRecordTarget& target, const ResponseTask& task, const Document& document)
{
    CommandDescription description(QObject::tr("New %1 task for %2").arg(task.category(), target.displayName()),
                                   describeTask(task, document));
    return std::make_shared<AddTaskCommand>(target, task, description);
}

AddTaskCommand::AddTaskCommand(const FamilyRecordTarget& target, const ResponseTask& task, const CommandDescription& description)
    : UndoableCommand(description)
    , m_target(target)
    , m_task(task)
{
}

CommandOutcome AddTaskCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.findEmergencyResponseById(m_target.responseId());
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    FamilyResponseRecord record = m_target.recordIn(*response);
    if (record.findTask(m_task.id()))
    {
        return CommandOutcome::Applied;
    }
    if (!assigneeExists(m_task.assignment(), document))
    {
        return CommandOutcome::Conflicted;
    }
    record.insertTask(m_task);
    writeRecord(document, *response, record);
    return CommandOutcome::Applied;
}

UndoableCommandPtr AddTaskCommand::inverse() const
{
    return std::make_shared<DeleteTaskCommand>(m_target, m_task, description().undone());
}

QJsonObject AddTaskCommand::toJson() const
{
    QJsonObject json;
    json["target"] = m_target.toJson();
    json["task"] = m_task.toJson();
    return json;
}

DocumentChange AddTaskCommand::documentChange() const
{
    return recordChange(m_target);
}

CommandPtr AddTaskCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<AddTaskCommand>(FamilyRecordTarget::fromJson(json["target"].toObject()),
                                            ResponseTask::fromJson(json["task"].toObject()),
                                            description);
}

// ----------------------------------------------------------------------------
// UpdateTaskCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr UpdateTaskCommand::fromEdit(const FamilyRecordTarget& target,
                                               const ResponseTask& opened,
                                               const ResponseTask& saved,
                                               const Document& document)
{
    std::optional<FieldChange<QString>> category = changeBetween(opened.category(), saved.category());
    std::optional<FieldChange<QString>> taskDescription = changeBetween(opened.description(), saved.description());
    std::optional<FieldChange<std::optional<TaskAssignment>>> assignment = changeBetween(opened.assignment(), saved.assignment());
    std::optional<FieldChange<std::optional<TaskResolution>>> resolution = changeBetween(opened.resolution(), saved.resolution());
    if (!category.has_value() && !taskDescription.has_value() && !assignment.has_value() && !resolution.has_value())
    {
        return nullptr;
    }

    QStringList lines;
    if (category.has_value())
    {
        lines.append(QObject::tr("category: %1").arg(category->after()));
    }
    if (taskDescription.has_value())
    {
        lines.append(QObject::tr("description: %1").arg(orNone(taskDescription->after())));
    }
    if (assignment.has_value())
    {
        lines.append(describeAssignment(assignment->after(), document));
    }
    if (resolution.has_value())
    {
        lines.append(describeResolution(resolution->after()));
    }
    CommandDescription description(QObject::tr("%1 task for %2").arg(opened.category(), target.displayName()), lines);
    return std::make_shared<UpdateTaskCommand>(target.responseId(), target.familyId(), opened.id(),
                                               category, taskDescription, assignment, resolution, description);
}

UpdateTaskCommand::UpdateTaskCommand(const EmergencyResponseId& responseId,
                                     const FamilyId& familyId,
                                     const TaskId& taskId,
                                     const std::optional<FieldChange<QString>>& category,
                                     const std::optional<FieldChange<QString>>& taskDescription,
                                     const std::optional<FieldChange<std::optional<TaskAssignment>>>& assignment,
                                     const std::optional<FieldChange<std::optional<TaskResolution>>>& resolution,
                                     const CommandDescription& description)
    : UndoableCommand(description)
    , m_responseId(responseId)
    , m_familyId(familyId)
    , m_taskId(taskId)
    , m_category(category)
    , m_taskDescription(taskDescription)
    , m_assignment(assignment)
    , m_resolution(resolution)
{
}

CommandOutcome UpdateTaskCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.findEmergencyResponseById(m_responseId);
    const FamilyResponseRecord* record = response.has_value() ? response->findRecord(m_familyId) : nullptr;
    const ResponseTask* task = record ? record->findTask(m_taskId) : nullptr;
    if (!task
        || !changeHolds(m_category, task->category())
        || !changeHolds(m_taskDescription, task->description())
        || !changeHolds(m_assignment, task->assignment())
        || !changeHolds(m_resolution, task->resolution()))
    {
        return CommandOutcome::Conflicted;
    }
    std::optional<TaskAssignment> assignment = changedValue(m_assignment, task->assignment());
    if (!assigneeExists(assignment, document))
    {
        return CommandOutcome::Conflicted;
    }

    ResponseTask updated = *task;
    updated.setCategory(changedValue(m_category, task->category()));
    updated.setDescription(changedValue(m_taskDescription, task->description()));
    updated.setAssignment(assignment);
    updated.setResolution(changedValue(m_resolution, task->resolution()));
    FamilyResponseRecord changed = *record;
    changed.setTask(updated);
    writeRecord(document, *response, changed);
    return CommandOutcome::Applied;
}

UndoableCommandPtr UpdateTaskCommand::inverse() const
{
    return std::make_shared<UpdateTaskCommand>(m_responseId,
                                               m_familyId,
                                               m_taskId,
                                               changeSwapped(m_category),
                                               changeSwapped(m_taskDescription),
                                               changeSwapped(m_assignment),
                                               changeSwapped(m_resolution),
                                               description().undone());
}

QJsonObject UpdateTaskCommand::toJson() const
{
    QJsonObject json;
    json["responseId"] = m_responseId.toString();
    json["familyId"] = m_familyId.toString();
    json["taskId"] = m_taskId.toString();
    writeChange(json, "category", m_category);
    writeChange(json, "taskDescription", m_taskDescription);
    writeChange(json, "assignment", m_assignment);
    writeChange(json, "resolution", m_resolution);
    return json;
}

DocumentChange UpdateTaskCommand::documentChange() const
{
    return DocumentChange::emergencyResponse().updated(m_responseId, m_familyId);
}

CommandPtr UpdateTaskCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<UpdateTaskCommand>(EmergencyResponseId::fromString(json["responseId"].toString()),
                                               FamilyId::fromString(json["familyId"].toString()),
                                               TaskId::fromString(json["taskId"].toString()),
                                               readChange<QString>(json, "category"),
                                               readChange<QString>(json, "taskDescription"),
                                               readChange<std::optional<TaskAssignment>>(json, "assignment"),
                                               readChange<std::optional<TaskResolution>>(json, "resolution"),
                                               description);
}

// ----------------------------------------------------------------------------
// DeleteTaskCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr DeleteTaskCommand::fromSeen(const FamilyRecordTarget& target, const ResponseTask& task, const Document& document)
{
    CommandDescription description(QObject::tr("Delete %1 task for %2").arg(task.category(), target.displayName()),
                                   describeTask(task, document));
    return std::make_shared<DeleteTaskCommand>(target, task, description);
}

DeleteTaskCommand::DeleteTaskCommand(const FamilyRecordTarget& target, const ResponseTask& task, const CommandDescription& description)
    : UndoableCommand(description)
    , m_target(target)
    , m_task(task)
{
}

CommandOutcome DeleteTaskCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.findEmergencyResponseById(m_target.responseId());
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    const FamilyResponseRecord* record = response->findRecord(m_target.familyId());
    const ResponseTask* task = record ? record->findTask(m_task.id()) : nullptr;
    if (!task)
    {
        return CommandOutcome::Applied;
    }
    if (*task != m_task)
    {
        return CommandOutcome::Conflicted;
    }
    FamilyResponseRecord changed = *record;
    changed.removeTask(m_task.id());
    writeRecord(document, *response, changed);
    return CommandOutcome::Applied;
}

UndoableCommandPtr DeleteTaskCommand::inverse() const
{
    return std::make_shared<AddTaskCommand>(m_target, m_task, description().undone());
}

QJsonObject DeleteTaskCommand::toJson() const
{
    QJsonObject json;
    json["target"] = m_target.toJson();
    json["task"] = m_task.toJson();
    return json;
}

DocumentChange DeleteTaskCommand::documentChange() const
{
    return recordChange(m_target);
}

CommandPtr DeleteTaskCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<DeleteTaskCommand>(FamilyRecordTarget::fromJson(json["target"].toObject()),
                                               ResponseTask::fromJson(json["task"].toObject()),
                                               description);
}

// ----------------------------------------------------------------------------
// AddTaskCategoryCommand
// ----------------------------------------------------------------------------

CommandPtr AddTaskCategoryCommand::fromNew(const EmergencyResponseId& responseId, const QString& category)
{
    CommandDescription description(QObject::tr("New task category: %1").arg(category), {});
    return std::make_shared<AddTaskCategoryCommand>(responseId, category, description);
}

AddTaskCategoryCommand::AddTaskCategoryCommand(const EmergencyResponseId& responseId,
                                               const QString& category,
                                               const CommandDescription& description)
    : Command(description)
    , m_responseId(responseId)
    , m_category(category)
{
}

CommandOutcome AddTaskCategoryCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.findEmergencyResponseById(m_responseId);
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    if (!response->taskCategories().contains(m_category))
    {
        response->addTaskCategory(m_category);
        document.setEmergencyResponse(*response);
    }
    return CommandOutcome::Applied;
}

QJsonObject AddTaskCategoryCommand::toJson() const
{
    QJsonObject json;
    json["responseId"] = m_responseId.toString();
    json["category"] = m_category;
    return json;
}

DocumentChange AddTaskCategoryCommand::documentChange() const
{
    return DocumentChange::emergencyResponse().updated(m_responseId);
}

CommandPtr AddTaskCategoryCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<AddTaskCategoryCommand>(EmergencyResponseId::fromString(json["responseId"].toString()),
                                                    json["category"].toString(),
                                                    description);
}
