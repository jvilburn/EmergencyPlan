#pragma once

#include "EmergencyResponse.h"

#include <optional>

class QWidget;

/// The user's actions on the shown emergency response that more than one view offers. Each
/// takes the task as it stands and the shown response's id when it starts, before any dialog,
/// builds its command from them, and commits it through DocumentManager. Each does nothing
/// when no response is shown or the task is gone. A removal takes the task as the caller saw
/// it, before its confirmation.
namespace EmergencyActions
{

/// The shown response's task with that id, as the document holds it now.
std::optional<ResponseTask> findTask(const TaskId& taskId);

/// Adds attempt to the shown response.
void addContactAttempt(const ContactAttempt& attempt);

/// Asks for a new task for the family and adds it. The task added, or none when the user
/// cancelled or it did not apply.
std::optional<ResponseTask> addTask(const FamilyId& familyId, QWidget* parent);

/// Opens the task dialog on the task and commits the edit. The task as saved, or none when
/// the user cancelled or it did not apply.
std::optional<ResponseTask> editTask(const TaskId& taskId, QWidget* parent);

/// Asks how the task's assignee was told and records it on the assignment.
void notifyAssignee(const TaskId& taskId, QWidget* parent);

/// Asks for resolution notes and resolves the task.
void resolveTask(const TaskId& taskId, QWidget* parent);

/// Removes the task as seen.
void deleteTask(const ResponseTask& seen);

}  // namespace EmergencyActions
