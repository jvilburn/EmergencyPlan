#include "EmergencyActions.h"
#include "ContactAttemptDialog.h"
#include "DocumentManager.h"
#include "EmergencyCommands.h"
#include "EmergencyManager.h"
#include "NotifyDialog.h"
#include "TaskDialog.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QObject>

namespace
{

const Document& document()
{
    return DocumentManager::instance()->document();
}

bool commitApplied(CommandPtr command)
{
    return DocumentManager::instance()->executeCommand(command).status == CommitStatus::Applied;
}

/// The task with that id in the emergency in progress, as the document holds it now, or
/// null.
const ResponseTask* taskInProgress(const TaskId& taskId)
{
    EmergencyManager* manager = EmergencyManager::instance();
    return manager->isActive() ? manager->response().findTask(taskId) : nullptr;
}

/// Offers to notify the task's assignee when it has one who has not been told.
void offerToNotify(const ResponseTask& task, QWidget* parent)
{
    if (task.isAssigned() && !task.isNotified())
    {
        EmergencyActions::notifyAssignee(task.id(), parent);
    }
}

}  // namespace

void EmergencyActions::markContact(const FamilyId& familyId, ContactOutcome outcome)
{
    std::optional<Family> family = document().findFamilyById(familyId);
    if (!family)
    {
        return;
    }
    ContactAttempt attempt = ContactAttempt::create(*family, std::nullopt, std::nullopt, outcome, QString());
    DocumentManager::instance()->executeCommand(AddContactAttemptCommand::fromNew(attempt));
}

void EmergencyActions::logContact(const FamilyId& familyId, QWidget* parent)
{
    std::optional<Family> family = document().findFamilyById(familyId);
    if (!family)
    {
        return;
    }
    ContactAttemptDialog dialog(*family, parent);
    if (dialog.exec() != QDialog::Accepted || !dialog.result())
    {
        return;
    }
    DocumentManager::instance()->executeCommand(AddContactAttemptCommand::fromNew(*dialog.result()));
}

void EmergencyActions::addTask(const FamilyId& familyId, QWidget* parent)
{
    std::optional<Family> family = document().findFamilyById(familyId);
    if (!family)
    {
        return;
    }
    TaskDialog dialog(*family, parent);
    if (dialog.exec() != QDialog::Accepted || !dialog.result())
    {
        return;
    }
    ResponseTask task = *dialog.result();
    if (commitApplied(AddTaskCommand::fromNew(task)))
    {
        offerToNotify(task, parent);
    }
}

void EmergencyActions::editTask(const TaskId& taskId, QWidget* parent)
{
    const ResponseTask* found = taskInProgress(taskId);
    if (!found)
    {
        return;
    }
    ResponseTask opened = *found;

    TaskDialog dialog(opened, parent);
    if (dialog.exec() != QDialog::Accepted || !dialog.result())
    {
        return;
    }
    ResponseTask saved = *dialog.result();
    if (commitApplied(UpdateTaskCommand::fromEdit(opened, saved)))
    {
        offerToNotify(saved, parent);
    }
}

void EmergencyActions::notifyAssignee(const TaskId& taskId, QWidget* parent)
{
    const ResponseTask* found = taskInProgress(taskId);
    if (!found || !found->assignment())
    {
        return;
    }
    ResponseTask opened = *found;

    NotifyDialog dialog(parent);
    if (dialog.exec() != QDialog::Accepted || !dialog.result())
    {
        return;
    }
    TaskAssignment notified = *opened.assignment();
    notified.setNotification(*dialog.result());
    ResponseTask saved = opened;
    saved.setAssignment(notified);
    DocumentManager::instance()->executeCommand(UpdateTaskCommand::fromEdit(opened, saved));
}

void EmergencyActions::resolveTask(const TaskId& taskId, QWidget* parent)
{
    const ResponseTask* found = taskInProgress(taskId);
    if (!found)
    {
        return;
    }
    ResponseTask opened = *found;

    bool ok = false;
    QString notes = QInputDialog::getText(
        parent, QObject::tr("Resolve Task"), QObject::tr("Resolution notes (optional):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
    {
        return;
    }
    ResponseTask saved = opened;
    saved.setResolution(TaskResolution::create(notes.trimmed()));
    DocumentManager::instance()->executeCommand(UpdateTaskCommand::fromEdit(opened, saved));
}

void EmergencyActions::deleteTask(const TaskId& taskId, QWidget* parent)
{
    const ResponseTask* found = taskInProgress(taskId);
    if (!found)
    {
        return;
    }
    // The task as the user sees it, which the removal takes as its start condition
    ResponseTask seen = *found;

    QMessageBox::StandardButton answer = QMessageBox::question(
        parent, QObject::tr("Delete Task"), QObject::tr("Delete this task?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
    {
        DocumentManager::instance()->executeCommand(DeleteTaskCommand::fromSeen(seen));
    }
}
