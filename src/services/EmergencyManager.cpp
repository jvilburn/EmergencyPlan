#include "EmergencyManager.h"
#include "DocumentManager.h"
#include "DocumentChange.h"

EmergencyManager* EmergencyManager::s_instance = nullptr;

EmergencyManager::EmergencyManager(QObject* parent)
    : QObject(parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &EmergencyManager::onDocumentChanged);
}

EmergencyManager* EmergencyManager::instance()
{
    return s_instance;
}

void EmergencyManager::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change)
    syncFamilies();
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
    const QHash<FamilyId, Family>& families = DocumentManager::instance()->document().families();
    for (auto it = families.constBegin(); it != families.constEnd(); ++it)
    {
        const Family& family = it.value();
        FamilyResponseRecord record = FamilyResponseRecord::create(
            family.id(),
            family.displayName(),
            family.address().full());
        m_response->addFamilyRecord(record);
    }

    emit emergencyStarted();
    emit responseDataChanged();
}

void EmergencyManager::endEmergency()
{
    if (!m_response.has_value())
    {
        return;
    }

    m_response.reset();
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
    ResponseTask* task = record->mutableTask(taskId);
    if (!task)
    {
        return;
    }
    task->resolve(notes);
    emit familyStatusChanged(familyId);
    emit responseDataChanged();
}

void EmergencyManager::reopenTask(const FamilyId& familyId, const TaskId& taskId)
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
    ResponseTask* task = record->mutableTask(taskId);
    if (!task)
    {
        return;
    }
    task->reopen();
    emit familyStatusChanged(familyId);
    emit responseDataChanged();
}

// ============================================================================
// Task assignment
// ============================================================================

void EmergencyManager::assignTaskToTeam(const FamilyId& familyId, const TaskId& taskId, const TeamId& teamId, const QString& notes)
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
    ResponseTask* task = record->mutableTask(taskId);
    if (!task)
    {
        return;
    }
    task->assignToTeam(teamId, notes);
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
    ResponseTask* task = record->mutableTask(taskId);
    if (!task)
    {
        return;
    }
    task->setNotification(notification);
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

void EmergencyManager::syncFamilies()
{
    if (!m_response.has_value())
    {
        return;
    }

    const QHash<FamilyId, Family>& families = DocumentManager::instance()->document().families();
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
