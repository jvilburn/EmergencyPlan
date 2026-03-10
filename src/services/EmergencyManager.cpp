#include "EmergencyManager.h"
#include "DocumentManager.h"
#include "DocumentChange.h"

EmergencyManager::EmergencyManager(DocumentManager* documentManager, QObject* parent)
    : QObject(parent)
    , m_documentManager(documentManager)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, [this](const DocumentChange& change)
    {
        if (change.action == ChangeAction::Full)
        {
            syncFromDocument();
        }
        else if (m_response.has_value())
        {
            syncFamilies();
        }
    });
}

// ============================================================================
// Lifecycle
// ============================================================================

bool EmergencyManager::isActive() const
{
    return m_response.has_value();
}

void EmergencyManager::startEmergency(const QString& name)
{
    if (m_response.has_value())
    {
        return;
    }

    m_response = EmergencyResponse::create(name);

    // Create FamilyResponseRecord for each family in the document
    const QHash<FamilyId, Family>& families = m_documentManager->document().families();
    for (auto it = families.constBegin(); it != families.constEnd(); ++it)
    {
        const Family& family = it.value();
        FamilyResponseRecord record = FamilyResponseRecord::create(
            family.id(),
            family.displayName(),
            family.address().full());
        m_response->addFamilyRecord(record);
    }

    persistResponseData();
    emit emergencyStarted();
    emit responseDataChanged();
}

void EmergencyManager::endEmergency(bool archive)
{
    if (!m_response.has_value())
    {
        return;
    }

    if (archive)
    {
        m_response->setEndedAt(QDateTime::currentDateTimeUtc());
        // Update document with final response data before archiving
        m_documentManager->setEmergencyResponse(m_response);
        // Archive save is handled by Task 1.5 — for now just fall through
    }

    // Clear response data from main document
    m_response.reset();
    m_documentManager->setEmergencyResponse(std::nullopt);
    emit emergencyEnded();
    emit responseDataChanged();
}

// ============================================================================
// Read access
// ============================================================================

const EmergencyResponse& EmergencyManager::response() const
{
    Q_ASSERT(m_response.has_value());
    return *m_response;
}

const FamilyResponseRecord* EmergencyManager::recordForFamily(const FamilyId& familyId) const
{
    if (!m_response.has_value())
    {
        return nullptr;
    }
    return m_response->findRecord(familyId);
}

EffectiveContactStatus EmergencyManager::familyStatus(const FamilyId& familyId) const
{
    if (!m_response.has_value())
    {
        return EffectiveContactStatus::NotContacted;
    }
    const FamilyResponseRecord* record = m_response->findRecord(familyId);
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
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    record->setContactStatus(status);
    persistResponseData();
    emit familyStatusChanged(familyId);
    emit responseDataChanged();
}

// ============================================================================
// Contact attempts
// ============================================================================

void EmergencyManager::addContactAttempt(const FamilyId& familyId, const ContactAttempt& attempt)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    record->addContactAttempt(attempt);
    persistResponseData();
    emit familyStatusChanged(familyId);
    emit responseDataChanged();
}

void EmergencyManager::removeContactAttempt(const FamilyId& familyId, const ContactAttemptId& attemptId)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    record->removeContactAttempt(attemptId);
    persistResponseData();
    emit familyStatusChanged(familyId);
    emit responseDataChanged();
}

// ============================================================================
// Tasks
// ============================================================================

void EmergencyManager::addTask(const FamilyId& familyId, const ResponseTask& task)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    record->addTask(task);
    persistResponseData();
    emit familyStatusChanged(familyId);
    emit responseDataChanged();
}

void EmergencyManager::updateTask(const FamilyId& familyId, const ResponseTask& task)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    record->updateTask(task);
    persistResponseData();
    emit familyStatusChanged(familyId);
    emit responseDataChanged();
}

void EmergencyManager::removeTask(const FamilyId& familyId, const TaskId& taskId)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    record->removeTask(taskId);
    persistResponseData();
    emit familyStatusChanged(familyId);
    emit responseDataChanged();
}

void EmergencyManager::resolveTask(const FamilyId& familyId, const TaskId& taskId, const QString& notes)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    QList<ResponseTask>& tasks = const_cast<QList<ResponseTask>&>(record->tasks());
    for (int i = 0; i < tasks.size(); ++i)
    {
        if (tasks[i].id() == taskId)
        {
            tasks[i].resolve(notes);
            break;
        }
    }
    persistResponseData();
    emit familyStatusChanged(familyId);
    emit responseDataChanged();
}

// ============================================================================
// Task assignment
// ============================================================================

void EmergencyManager::assignTaskToTeam(const FamilyId& familyId, const TaskId& taskId, const TeamId& teamId)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    QList<ResponseTask>& tasks = const_cast<QList<ResponseTask>&>(record->tasks());
    for (int i = 0; i < tasks.size(); ++i)
    {
        if (tasks[i].id() == taskId)
        {
            tasks[i].assignToTeam(teamId, QString());
            break;
        }
    }
    persistResponseData();
    emit responseDataChanged();
}

void EmergencyManager::assignTaskToPerson(const FamilyId& familyId, const TaskId& taskId, const PersonId& personId)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    QList<ResponseTask>& tasks = const_cast<QList<ResponseTask>&>(record->tasks());
    for (int i = 0; i < tasks.size(); ++i)
    {
        if (tasks[i].id() == taskId)
        {
            tasks[i].assignToPerson(personId, QString());
            break;
        }
    }
    persistResponseData();
    emit responseDataChanged();
}

void EmergencyManager::unassignTask(const FamilyId& familyId, const TaskId& taskId)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    QList<ResponseTask>& tasks = const_cast<QList<ResponseTask>&>(record->tasks());
    for (int i = 0; i < tasks.size(); ++i)
    {
        if (tasks[i].id() == taskId)
        {
            tasks[i].clearAssignment();
            break;
        }
    }
    persistResponseData();
    emit responseDataChanged();
}

// ============================================================================
// Task notification
// ============================================================================

void EmergencyManager::notifyAssignee(const FamilyId& familyId, const TaskId& taskId, const TaskNotification& notification)
{
    if (!m_response.has_value())
    {
        return;
    }
    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }
    QList<ResponseTask>& tasks = const_cast<QList<ResponseTask>&>(record->tasks());
    for (int i = 0; i < tasks.size(); ++i)
    {
        if (tasks[i].id() == taskId)
        {
            tasks[i].setNotification(notification);
            break;
        }
    }
    persistResponseData();
    emit responseDataChanged();
}

// ============================================================================
// Task categories
// ============================================================================

QStringList EmergencyManager::taskCategories() const
{
    if (!m_response.has_value())
    {
        return {};
    }
    return m_response->taskCategories();
}

void EmergencyManager::addTaskCategory(const QString& category)
{
    if (!m_response.has_value())
    {
        return;
    }
    m_response->addTaskCategory(category);
    persistResponseData();
}

// ============================================================================
// Statistics
// ============================================================================

int EmergencyManager::totalFamilies() const
{
    if (!m_response.has_value())
    {
        return 0;
    }
    return m_response->totalFamilies();
}

int EmergencyManager::countByStatus(EffectiveContactStatus status) const
{
    if (!m_response.has_value())
    {
        return 0;
    }
    return m_response->countByStatus(status);
}

// ============================================================================
// Private
// ============================================================================

void EmergencyManager::persistResponseData()
{
    m_documentManager->setEmergencyResponse(m_response);
}

void EmergencyManager::syncFromDocument()
{
    const std::optional<EmergencyResponse>& response = m_documentManager->document().emergencyResponse();
    if (response.has_value())
    {
        m_response = response;
        syncFamilies();
        emit emergencyStarted();
    }
    else if (m_response.has_value())
    {
        m_response.reset();
        emit emergencyEnded();
    }
}

void EmergencyManager::syncFamilies()
{
    if (!m_response.has_value())
    {
        return;
    }

    const QHash<FamilyId, Family>& families = m_documentManager->document().families();
    const QHash<FamilyId, FamilyResponseRecord>& records = m_response->familyRecords();

    // Add records for families that don't have one yet
    for (auto it = families.constBegin(); it != families.constEnd(); ++it)
    {
        if (!records.contains(it.key()))
        {
            const Family& family = it.value();
            FamilyResponseRecord record = FamilyResponseRecord::create(
                family.id(),
                family.displayName(),
                family.address().full());
            m_response->addFamilyRecord(record);
        }
    }
    // Removed families: leave their records (snapshot data useful for current emergency)
}
