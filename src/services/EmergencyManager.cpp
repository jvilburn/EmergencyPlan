#include "EmergencyManager.h"
#include "DocumentManager.h"
#include "DocumentChange.h"

#include <algorithm>

using namespace std::placeholders;

namespace
{

/// A record for each family, as an emergency starts with or a family added during
/// one gets.
void addRecords(std::optional<EmergencyResponse>& response, const QList<Family>& families)
{
    if (!response.has_value())
    {
        return;
    }
    for (const Family& family : families)
    {
        response->addFamilyRecord(FamilyResponseRecord::create(family.id(),
                                                               family.displayName(),
                                                               family.address().full()));
    }
}

void startResponse(std::optional<EmergencyResponse>& response, const QString& name, const QList<Family>& families)
{
    response = EmergencyResponse::create(name);
    addRecords(response, families);
}

} // namespace

EmergencyManager* EmergencyManager::s_instance = nullptr;

EmergencyManager::EmergencyManager(QObject* parent)
    : QObject(parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_wasActive = current().has_value();
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &EmergencyManager::onDocumentChanged);
}

EmergencyManager::~EmergencyManager()
{
    s_instance = nullptr;
}

EmergencyManager* EmergencyManager::instance()
{
    return s_instance;
}

void EmergencyManager::onDocumentChanged(const DocumentChange& change)
{
    // A copy shown read-only takes no records.
    const bool active = current().has_value();
    if (active && !DocumentManager::instance()->copy().has_value())
    {
        syncFamilies();
    }

    if (active != m_wasActive)
    {
        m_wasActive = active;
        if (active)
        {
            emit emergencyStarted();
        }
        else
        {
            emit emergencyEnded();
        }
        emit responseDataChanged();
    }
    else if (active && change.action == ChangeAction::Full)
    {
        // A replaced document may carry the other side's records.
        emit responseDataChanged();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool EmergencyManager::isActive() const
{
    return current().has_value();
}

bool EmergencyManager::startEmergency(const QString& name)
{
    if (current().has_value())
    {
        return false;
    }

    // A record for each family in the document.
    const QList<Family> families = DocumentManager::instance()->document().families().values();
    const bool started = DocumentManager::instance()->editResponse(std::bind(&startResponse, _1, name, families));
    if (!started)
    {
        return false;
    }

    m_wasActive = true;
    emit emergencyStarted();
    emit responseDataChanged();
    return true;
}

bool EmergencyManager::endEmergency()
{
    if (!current().has_value())
    {
        return false;
    }

    if (!DocumentManager::instance()->editResponse([](std::optional<EmergencyResponse>& response) { response.reset(); }))
    {
        return false;
    }

    m_wasActive = false;
    emit emergencyEnded();
    emit responseDataChanged();
    return true;
}

// ============================================================================
// Read access
// ============================================================================

const EmergencyResponse& EmergencyManager::response() const
{
    Q_ASSERT(current().has_value());
    return *current();
}

const FamilyResponseRecord* EmergencyManager::recordForFamily(const FamilyId& familyId) const
{
    if (!current().has_value())
    {
        return nullptr;
    }
    return current()->findRecord(familyId);
}

EffectiveContactStatus EmergencyManager::familyStatus(const FamilyId& familyId) const
{
    const FamilyResponseRecord* record = recordForFamily(familyId);
    if (!record)
    {
        return EffectiveContactStatus::NotContacted;
    }
    return record->effectiveStatus();
}

// ============================================================================
// Contact status
// ============================================================================

void EmergencyManager::setContactStatus(const FamilyId& familyId, ContactStatus status)
{
    if (changeRecord(familyId, [&](FamilyResponseRecord& record) { record.setContactStatus(status); }))
    {
        emit familyStatusChanged(familyId);
        emit responseDataChanged();
    }
}

// ============================================================================
// Contact attempts
// ============================================================================

void EmergencyManager::addContactAttempt(const FamilyId& familyId, const ContactAttempt& attempt)
{
    if (changeRecord(familyId, [&](FamilyResponseRecord& record) { record.addContactAttempt(attempt); }))
    {
        emit familyStatusChanged(familyId);
        emit responseDataChanged();
    }
}

void EmergencyManager::removeContactAttempt(const FamilyId& familyId, const ContactAttemptId& attemptId)
{
    if (changeRecord(familyId, [&](FamilyResponseRecord& record) { record.removeContactAttempt(attemptId); }))
    {
        emit familyStatusChanged(familyId);
        emit responseDataChanged();
    }
}

// ============================================================================
// Tasks
// ============================================================================

void EmergencyManager::addTask(const FamilyId& familyId, const ResponseTask& task)
{
    if (changeRecord(familyId, [&](FamilyResponseRecord& record) { record.addTask(task); }))
    {
        emit familyStatusChanged(familyId);
        emit responseDataChanged();
    }
}

void EmergencyManager::updateTask(const FamilyId& familyId, const ResponseTask& task)
{
    if (changeRecord(familyId, [&](FamilyResponseRecord& record) { record.updateTask(task); }))
    {
        emit familyStatusChanged(familyId);
        emit responseDataChanged();
    }
}

void EmergencyManager::removeTask(const FamilyId& familyId, const TaskId& taskId)
{
    if (changeRecord(familyId, [&](FamilyResponseRecord& record) { record.removeTask(taskId); }))
    {
        emit familyStatusChanged(familyId);
        emit responseDataChanged();
    }
}

void EmergencyManager::resolveTask(const FamilyId& familyId, const TaskId& taskId, const QString& notes)
{
    if (changeTask(familyId, taskId, [&](ResponseTask& task) { task.resolve(notes); }))
    {
        emit familyStatusChanged(familyId);
        emit responseDataChanged();
    }
}

void EmergencyManager::reopenTask(const FamilyId& familyId, const TaskId& taskId)
{
    if (changeTask(familyId, taskId, [](ResponseTask& task) { task.reopen(); }))
    {
        emit familyStatusChanged(familyId);
        emit responseDataChanged();
    }
}

// ============================================================================
// Task assignment
// ============================================================================

void EmergencyManager::assignTaskToTeam(const FamilyId& familyId, const TaskId& taskId, const TeamId& teamId, const QString& notes)
{
    if (changeTask(familyId, taskId, [&](ResponseTask& task) { task.assignToTeam(teamId, notes); }))
    {
        emit responseDataChanged();
    }
}

void EmergencyManager::assignTaskToPerson(const FamilyId& familyId, const TaskId& taskId, const PersonId& personId, const QString& notes)
{
    if (changeTask(familyId, taskId, [&](ResponseTask& task) { task.assignToPerson(personId, notes); }))
    {
        emit responseDataChanged();
    }
}

void EmergencyManager::unassignTask(const FamilyId& familyId, const TaskId& taskId)
{
    if (changeTask(familyId, taskId, [](ResponseTask& task) { task.clearAssignment(); }))
    {
        emit responseDataChanged();
    }
}

// ============================================================================
// Task notification
// ============================================================================

void EmergencyManager::notifyAssignee(const FamilyId& familyId, const TaskId& taskId, const TaskNotification& notification)
{
    if (changeTask(familyId, taskId, [&](ResponseTask& task) { task.setNotification(notification); }))
    {
        emit responseDataChanged();
    }
}

// ============================================================================
// Task categories
// ============================================================================

QStringList EmergencyManager::taskCategories() const
{
    if (!current().has_value())
    {
        return {};
    }
    return current()->taskCategories();
}

void EmergencyManager::addTaskCategory(const QString& category)
{
    if (!current().has_value())
    {
        return;
    }
    DocumentManager::instance()->editResponse([&](std::optional<EmergencyResponse>& response)
    {
        if (response.has_value())
        {
            response->addTaskCategory(category);
        }
    });
}

// ============================================================================
// Statistics
// ============================================================================

int EmergencyManager::totalFamilies() const
{
    if (!current().has_value())
    {
        return 0;
    }
    return current()->totalFamilies();
}

int EmergencyManager::countByStatus(EffectiveContactStatus status) const
{
    if (!current().has_value())
    {
        return 0;
    }
    return current()->countByStatus(status);
}

// ============================================================================
// Private
// ============================================================================

const std::optional<EmergencyResponse>& EmergencyManager::current() const
{
    return DocumentManager::instance()->document().emergencyResponse();
}

bool EmergencyManager::changeRecord(const FamilyId& familyId,
                                    const std::function<void(FamilyResponseRecord&)>& change)
{
    if (recordForFamily(familyId) == nullptr)
    {
        return false;
    }

    return DocumentManager::instance()->editResponse([&](std::optional<EmergencyResponse>& response)
    {
        FamilyResponseRecord* record = response.has_value() ? response->mutableRecord(familyId) : nullptr;
        if (record != nullptr)
        {
            change(*record);
        }
    });
}

bool EmergencyManager::changeTask(const FamilyId& familyId,
                                  const TaskId& taskId,
                                  const std::function<void(ResponseTask&)>& change)
{
    const FamilyResponseRecord* existing = recordForFamily(familyId);
    if (existing == nullptr
        || std::none_of(existing->tasks().cbegin(),
                        existing->tasks().cend(),
                        [&](const ResponseTask& task) { return task.id() == taskId; }))
    {
        return false;
    }

    return changeRecord(familyId, [&](FamilyResponseRecord& record)
    {
        ResponseTask* task = record.mutableTask(taskId);
        if (task != nullptr)
        {
            change(*task);
        }
    });
}

void EmergencyManager::syncFamilies()
{
    const QHash<FamilyId, Family>& families = DocumentManager::instance()->document().families();
    const QHash<FamilyId, FamilyResponseRecord>& records = current()->familyRecords();

    // Add records for families that don't have one yet. Removed families keep
    // theirs: snapshot data useful for the current emergency.
    QList<Family> missing;
    for (auto it = families.constBegin(); it != families.constEnd(); ++it)
    {
        if (!records.contains(it.key()))
        {
            missing.append(it.value());
        }
    }
    if (missing.isEmpty())
    {
        return;
    }

    if (DocumentManager::instance()->editResponse(std::bind(&addRecords, _1, missing)))
    {
        emit responseDataChanged();
    }
}
