#include "EmergencyActions.h"
#include "DocumentManager.h"
#include "EmergencyCommands.h"
#include "EmergencyManager.h"
#include "NotifyDialog.h"
#include "TaskDialog.h"

#include <QInputDialog>
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

}  // namespace

std::optional<ResponseTask> EmergencyActions::findTask(const TaskId& taskId)
{
    EmergencyManager* manager = EmergencyManager::instance();
    if (!manager->isActive())
    {
        return std::nullopt;
    }
    const ResponseTask* task = manager->response().findTask(taskId);
    if (!task)
    {
        return std::nullopt;
    }
    return *task;
}

void EmergencyActions::addContactAttempt(const ContactAttempt& attempt)
{
    EmergencyManager* manager = EmergencyManager::instance();
    if (!manager->isActive())
    {
        return;
    }
    DocumentManager::instance()->executeCommand(AddContactAttemptCommand::fromNew(*manager->shownId(), attempt, document()));
}

std::optional<ResponseTask> EmergencyActions::addTask(const FamilyId& familyId, QWidget* parent)
{
    EmergencyManager* manager = EmergencyManager::instance();
    if (!manager->isActive())
    {
        return std::nullopt;
    }
    EmergencyResponseId responseId = *manager->shownId();

    TaskDialog dialog(familyId, parent);
    if (dialog.exec() != QDialog::Accepted || !dialog.result())
    {
        return std::nullopt;
    }
    ResponseTask task = *dialog.result();
    if (!commitApplied(AddTaskCommand::fromNew(responseId, task, document())))
    {
        return std::nullopt;
    }
    return task;
}

std::optional<ResponseTask> EmergencyActions::editTask(const TaskId& taskId, QWidget* parent)
{
    std::optional<ResponseTask> opened = findTask(taskId);
    if (!opened)
    {
        return std::nullopt;
    }
    EmergencyResponseId responseId = *EmergencyManager::instance()->shownId();

    TaskDialog dialog(opened->familyId(), parent);
    dialog.setTask(*opened);
    if (dialog.exec() != QDialog::Accepted || !dialog.result())
    {
        return std::nullopt;
    }
    ResponseTask saved = *dialog.result();
    if (!commitApplied(UpdateTaskCommand::fromEdit(responseId, *opened, saved, document())))
    {
        return std::nullopt;
    }
    return saved;
}

void EmergencyActions::notifyAssignee(const TaskId& taskId, QWidget* parent)
{
    std::optional<ResponseTask> opened = findTask(taskId);
    if (!opened || !opened->assignment())
    {
        return;
    }
    EmergencyResponseId responseId = *EmergencyManager::instance()->shownId();

    NotifyDialog dialog(parent);
    if (dialog.exec() != QDialog::Accepted || !dialog.result())
    {
        return;
    }
    TaskAssignment notified = *opened->assignment();
    notified.setNotification(*dialog.result());
    ResponseTask saved = *opened;
    saved.setAssignment(notified);
    DocumentManager::instance()->executeCommand(UpdateTaskCommand::fromEdit(responseId, *opened, saved, document()));
}

void EmergencyActions::resolveTask(const TaskId& taskId, QWidget* parent)
{
    std::optional<ResponseTask> opened = findTask(taskId);
    if (!opened)
    {
        return;
    }
    EmergencyResponseId responseId = *EmergencyManager::instance()->shownId();

    bool ok = false;
    QString notes = QInputDialog::getText(
        parent, QObject::tr("Resolve Task"), QObject::tr("Resolution notes (optional):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
    {
        return;
    }
    ResponseTask saved = *opened;
    saved.setResolution(TaskResolution::create(notes.trimmed()));
    DocumentManager::instance()->executeCommand(UpdateTaskCommand::fromEdit(responseId, *opened, saved, document()));
}

void EmergencyActions::deleteTask(const ResponseTask& seen)
{
    EmergencyManager* manager = EmergencyManager::instance();
    if (!manager->isActive())
    {
        return;
    }
    DocumentManager::instance()->executeCommand(DeleteTaskCommand::fromSeen(*manager->shownId(), seen, document()));
}
