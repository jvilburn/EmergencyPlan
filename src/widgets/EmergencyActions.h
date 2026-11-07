#pragma once

#include "EmergencyResponse.h"

class QWidget;

/// The user's actions on the emergency in progress, each a whole flow: its dialogs, then the
/// command it builds and commits through DocumentManager. Every view that offers an action
/// calls the one here, so it behaves the same wherever it starts. An action on a family or a
/// task takes it as the document holds it when the action starts, before any dialog, and
/// does nothing when the family is gone, or when no emergency is in progress or the task is
/// gone.
namespace EmergencyActions
{

/// Records an attempt on the family carrying only outcome, as the OK and Unable to Reach
/// buttons do.
void markContact(const FamilyId& familyId, ContactOutcome outcome);

/// Asks for a contact attempt on the family and records it.
void logContact(const FamilyId& familyId, QWidget* parent);

/// Asks for a new task for the family and adds it, then offers to notify its assignee.
void addTask(const FamilyId& familyId, QWidget* parent);

/// Opens the task dialog on the task and commits the edit, then offers to notify its
/// assignee when they have not been told.
void editTask(const TaskId& taskId, QWidget* parent);

/// Asks how the task's assignee was told and records it on the assignment.
void notifyAssignee(const TaskId& taskId, QWidget* parent);

/// Asks for resolution notes and resolves the task.
void resolveTask(const TaskId& taskId, QWidget* parent);

/// Asks to confirm, then removes the task as it stood when the user chose to delete it.
void deleteTask(const TaskId& taskId, QWidget* parent);

}  // namespace EmergencyActions
