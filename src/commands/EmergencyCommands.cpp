#include "EmergencyCommands.h"
#include "Document.h"
#include "FieldChangeJson.h"

#include <QObject>

// JSON for the task values UpdateTaskCommand changes, read and written through the templates
// in FieldChangeJson.h. A task with no assignment or resolution writes null for it.

QJsonValue toJsonValue(const std::optional<TaskAssignment>& value)
{
    if (!value.has_value())
    {
        return QJsonValue(QJsonValue::Null);
    }
    return value->toJson();
}

QJsonValue toJsonValue(const std::optional<TaskResolution>& value)
{
    if (!value.has_value())
    {
        return QJsonValue(QJsonValue::Null);
    }
    return value->toJson();
}

template<>
std::optional<TaskAssignment> readValue<std::optional<TaskAssignment>>(JsonReader& json, const QString& key)
{
    if (json.readNull(key))
    {
        return std::nullopt;
    }
    return TaskAssignment::fromJson(json.object(key));
}

template<>
std::optional<TaskResolution> readValue<std::optional<TaskResolution>>(JsonReader& json, const QString& key)
{
    if (json.readNull(key))
    {
        return std::nullopt;
    }
    return TaskResolution::fromJson(json.object(key));
}

namespace
{

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

QStringList describeAttempt(const ContactAttempt& attempt)
{
    QStringList lines;
    if (attempt.method().has_value())
    {
        lines.append(QObject::tr("method: %1").arg(contactMethodName(*attempt.method())));
    }
    if (attempt.who().has_value())
    {
        lines.append(QObject::tr("who: %1").arg(attempt.whoName()));
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

QStringList describeAssignment(const std::optional<TaskAssignment>& assignment)
{
    if (!assignment.has_value())
    {
        return {QObject::tr("assigned to: (none)")};
    }
    QStringList lines;
    lines.append(QObject::tr("assigned to: %1").arg(assignment->assigneeName()));
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
QStringList describeTask(const ResponseTask& task)
{
    QStringList lines;
    lines.append(QObject::tr("description: %1").arg(orNone(task.description())));
    if (task.assignment().has_value())
    {
        lines.append(describeAssignment(task.assignment()));
    }
    if (task.resolution().has_value())
    {
        lines.append(describeResolution(task.resolution()));
    }
    return lines;
}

}  // namespace

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
    if (!document.emergencyResponse().has_value())
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
    return DocumentChange::emergency().updated();
}

CommandPtr StartEmergencyCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    EmergencyResponse response = EmergencyResponse::fromJson(json.object("response"));
    json.finish();
    return std::make_shared<StartEmergencyCommand>(response, description);
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
    const std::optional<EmergencyResponse>& inProgress = document.emergencyResponse();
    if (!inProgress.has_value())
    {
        return CommandOutcome::Applied;
    }
    if (*inProgress != m_response)
    {
        return CommandOutcome::Conflicted;
    }
    document.setEmergencyResponse(std::nullopt);
    return CommandOutcome::Applied;
}

CommandPtr EndEmergencyCommand::inverse() const
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
    return DocumentChange::emergency().updated();
}

CommandPtr EndEmergencyCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    EmergencyResponse response = EmergencyResponse::fromJson(json.object("response"));
    json.finish();
    return std::make_shared<EndEmergencyCommand>(response, description);
}

// ----------------------------------------------------------------------------
// RestoreEmergencyCommand
// ----------------------------------------------------------------------------

RestoreEmergencyCommand::RestoreEmergencyCommand(const EmergencyResponse& response, const CommandDescription& description)
    : Command(description)
    , m_response(response)
{
}

CommandOutcome RestoreEmergencyCommand::execute(Document& document) const
{
    const std::optional<EmergencyResponse>& inProgress = document.emergencyResponse();
    if (!inProgress.has_value())
    {
        document.setEmergencyResponse(m_response);
        return CommandOutcome::Applied;
    }
    return *inProgress == m_response ? CommandOutcome::Applied : CommandOutcome::Conflicted;
}

QJsonObject RestoreEmergencyCommand::toJson() const
{
    QJsonObject json;
    json["response"] = m_response.toJson();
    return json;
}

DocumentChange RestoreEmergencyCommand::documentChange() const
{
    return DocumentChange::emergency().updated();
}

CommandPtr RestoreEmergencyCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    EmergencyResponse response = EmergencyResponse::fromJson(json.object("response"));
    json.finish();
    return std::make_shared<RestoreEmergencyCommand>(response, description);
}

// ----------------------------------------------------------------------------
// AddContactAttemptCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr AddContactAttemptCommand::fromNew(const ContactAttempt& attempt)
{
    CommandDescription description(QObject::tr("Contact with %1").arg(attempt.familyName()), describeAttempt(attempt));
    return std::make_shared<AddContactAttemptCommand>(attempt, description);
}

AddContactAttemptCommand::AddContactAttemptCommand(const ContactAttempt& attempt, const CommandDescription& description)
    : UndoableCommand(description)
    , m_attempt(attempt)
{
}

CommandOutcome AddContactAttemptCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.emergencyResponse();
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    if (!response->findContactAttempt(m_attempt.id()))
    {
        response->insertContactAttempt(m_attempt);
        document.setEmergencyResponse(response);
    }
    return CommandOutcome::Applied;
}

CommandPtr AddContactAttemptCommand::inverse() const
{
    return std::make_shared<DeleteContactAttemptCommand>(m_attempt, description().undone());
}

QJsonObject AddContactAttemptCommand::toJson() const
{
    QJsonObject json;
    json["attempt"] = m_attempt.toJson();
    return json;
}

DocumentChange AddContactAttemptCommand::documentChange() const
{
    return DocumentChange::emergency().updated(m_attempt.familyId());
}

CommandPtr AddContactAttemptCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    ContactAttempt attempt = ContactAttempt::fromJson(json.object("attempt"));
    json.finish();
    return std::make_shared<AddContactAttemptCommand>(attempt, description);
}

// ----------------------------------------------------------------------------
// DeleteContactAttemptCommand
// ----------------------------------------------------------------------------

DeleteContactAttemptCommand::DeleteContactAttemptCommand(const ContactAttempt& attempt, const CommandDescription& description)
    : Command(description)
    , m_attempt(attempt)
{
}

CommandOutcome DeleteContactAttemptCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.emergencyResponse();
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    if (response->findContactAttempt(m_attempt.id()))
    {
        response->removeContactAttempt(m_attempt.id());
        document.setEmergencyResponse(response);
    }
    return CommandOutcome::Applied;
}

QJsonObject DeleteContactAttemptCommand::toJson() const
{
    QJsonObject json;
    json["attempt"] = m_attempt.toJson();
    return json;
}

DocumentChange DeleteContactAttemptCommand::documentChange() const
{
    return DocumentChange::emergency().updated(m_attempt.familyId());
}

CommandPtr DeleteContactAttemptCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    ContactAttempt attempt = ContactAttempt::fromJson(json.object("attempt"));
    json.finish();
    return std::make_shared<DeleteContactAttemptCommand>(attempt, description);
}

// ----------------------------------------------------------------------------
// AddTaskCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr AddTaskCommand::fromNew(const ResponseTask& task)
{
    CommandDescription description(QObject::tr("New %1 task for %2").arg(task.category(), task.familyName()),
                                   describeTask(task));
    return std::make_shared<AddTaskCommand>(task, description);
}

AddTaskCommand::AddTaskCommand(const ResponseTask& task, const CommandDescription& description)
    : UndoableCommand(description)
    , m_task(task)
{
}

CommandOutcome AddTaskCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.emergencyResponse();
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    if (response->findTask(m_task.id()))
    {
        return CommandOutcome::Applied;
    }
    if (!m_task.isResolved() && !assigneeExists(m_task.assignment(), document))
    {
        return CommandOutcome::Conflicted;
    }
    response->insertTask(m_task);
    document.setEmergencyResponse(response);
    return CommandOutcome::Applied;
}

CommandPtr AddTaskCommand::inverse() const
{
    return std::make_shared<DeleteTaskCommand>(m_task, description().undone());
}

QJsonObject AddTaskCommand::toJson() const
{
    QJsonObject json;
    json["task"] = m_task.toJson();
    return json;
}

DocumentChange AddTaskCommand::documentChange() const
{
    return DocumentChange::emergency().updated(m_task.familyId());
}

CommandPtr AddTaskCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    ResponseTask task = ResponseTask::fromJson(json.object("task"));
    json.finish();
    return std::make_shared<AddTaskCommand>(task, description);
}

// ----------------------------------------------------------------------------
// UpdateTaskCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr UpdateTaskCommand::fromEdit(const ResponseTask& opened, const ResponseTask& saved)
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
        lines.append(describeAssignment(assignment->after()));
    }
    if (resolution.has_value())
    {
        lines.append(describeResolution(resolution->after()));
    }
    CommandDescription description(QObject::tr("%1 task for %2").arg(opened.category(), opened.familyName()), lines);
    return std::make_shared<UpdateTaskCommand>(opened.familyId(), opened.id(),
                                               category, taskDescription, assignment, resolution, description);
}

UndoableCommandPtr UpdateTaskCommand::reopening(const ResponseTask& task, const Document& document)
{
    ResponseTask reopened = task;
    reopened.setResolution(std::nullopt);
    if (!assigneeExists(reopened.assignment(), document))
    {
        reopened.setAssignment(std::nullopt);
    }
    return fromEdit(task, reopened);
}

UpdateTaskCommand::UpdateTaskCommand(const FamilyId& familyId,
                                     const TaskId& taskId,
                                     const std::optional<FieldChange<QString>>& category,
                                     const std::optional<FieldChange<QString>>& taskDescription,
                                     const std::optional<FieldChange<std::optional<TaskAssignment>>>& assignment,
                                     const std::optional<FieldChange<std::optional<TaskResolution>>>& resolution,
                                     const CommandDescription& description)
    : UndoableCommand(description)
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
    std::optional<EmergencyResponse> response = document.emergencyResponse();
    const ResponseTask* task = response.has_value() ? response->findTask(m_taskId) : nullptr;
    if (!task
        || !changeHolds(m_category, task->category())
        || !changeHolds(m_taskDescription, task->description())
        || !changeHolds(m_assignment, task->assignment())
        || !changeHolds(m_resolution, task->resolution()))
    {
        return CommandOutcome::Conflicted;
    }

    ResponseTask updated = *task;
    updated.setCategory(changedValue(m_category, task->category()));
    updated.setDescription(changedValue(m_taskDescription, task->description()));
    updated.setAssignment(changedValue(m_assignment, task->assignment()));
    updated.setResolution(changedValue(m_resolution, task->resolution()));
    if (!updated.isResolved() && !assigneeExists(updated.assignment(), document))
    {
        return CommandOutcome::Conflicted;
    }
    response->setTask(updated);
    document.setEmergencyResponse(response);
    return CommandOutcome::Applied;
}

CommandPtr UpdateTaskCommand::inverse() const
{
    return std::make_shared<UpdateTaskCommand>(m_familyId,
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
    return DocumentChange::emergency().updated(m_familyId);
}

CommandPtr UpdateTaskCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    FamilyId familyId = json.id<FamilyId>("familyId");
    TaskId taskId = json.id<TaskId>("taskId");
    std::optional<FieldChange<QString>> category = readChange<QString>(json, "category");
    std::optional<FieldChange<QString>> taskDescription = readChange<QString>(json, "taskDescription");
    std::optional<FieldChange<std::optional<TaskAssignment>>> assignment
        = readChange<std::optional<TaskAssignment>>(json, "assignment");
    std::optional<FieldChange<std::optional<TaskResolution>>> resolution
        = readChange<std::optional<TaskResolution>>(json, "resolution");
    json.finish();
    return std::make_shared<UpdateTaskCommand>(familyId, taskId, category, taskDescription, assignment, resolution,
                                               description);
}

// ----------------------------------------------------------------------------
// DeleteTaskCommand
// ----------------------------------------------------------------------------

UndoableCommandPtr DeleteTaskCommand::fromSeen(const ResponseTask& task)
{
    CommandDescription description(QObject::tr("Delete %1 task for %2").arg(task.category(), task.familyName()),
                                   describeTask(task));
    return std::make_shared<DeleteTaskCommand>(task, description);
}

DeleteTaskCommand::DeleteTaskCommand(const ResponseTask& task, const CommandDescription& description)
    : UndoableCommand(description)
    , m_task(task)
{
}

CommandOutcome DeleteTaskCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.emergencyResponse();
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    const ResponseTask* task = response->findTask(m_task.id());
    if (!task)
    {
        return CommandOutcome::Applied;
    }
    if (*task != m_task)
    {
        return CommandOutcome::Conflicted;
    }
    response->removeTask(m_task.id());
    document.setEmergencyResponse(response);
    return CommandOutcome::Applied;
}

CommandPtr DeleteTaskCommand::inverse() const
{
    return std::make_shared<AddTaskCommand>(m_task, description().undone());
}

QJsonObject DeleteTaskCommand::toJson() const
{
    QJsonObject json;
    json["task"] = m_task.toJson();
    return json;
}

DocumentChange DeleteTaskCommand::documentChange() const
{
    return DocumentChange::emergency().updated(m_task.familyId());
}

CommandPtr DeleteTaskCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    ResponseTask task = ResponseTask::fromJson(json.object("task"));
    json.finish();
    return std::make_shared<DeleteTaskCommand>(task, description);
}

// ----------------------------------------------------------------------------
// AddTaskCategoryCommand
// ----------------------------------------------------------------------------

CommandPtr AddTaskCategoryCommand::fromNew(const QString& category)
{
    CommandDescription description(QObject::tr("New task category: %1").arg(category), {});
    return std::make_shared<AddTaskCategoryCommand>(category, description);
}

AddTaskCategoryCommand::AddTaskCategoryCommand(const QString& category, const CommandDescription& description)
    : Command(description)
    , m_category(category)
{
}

CommandOutcome AddTaskCategoryCommand::execute(Document& document) const
{
    std::optional<EmergencyResponse> response = document.emergencyResponse();
    if (!response.has_value())
    {
        return CommandOutcome::Conflicted;
    }
    if (!response->taskCategories().contains(m_category))
    {
        response->addTaskCategory(m_category);
        document.setEmergencyResponse(response);
    }
    return CommandOutcome::Applied;
}

QJsonObject AddTaskCategoryCommand::toJson() const
{
    QJsonObject json;
    json["category"] = m_category;
    return json;
}

DocumentChange AddTaskCategoryCommand::documentChange() const
{
    return DocumentChange::emergency().updated();
}

CommandPtr AddTaskCategoryCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QString category = json.string("category");
    json.finish();
    return std::make_shared<AddTaskCategoryCommand>(category, description);
}
