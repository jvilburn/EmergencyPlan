#include "EmergencyManager.h"
#include "DocumentManager.h"
#include "DocumentChange.h"
#include "JsonService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>

EmergencyManager::EmergencyManager(DocumentManager* documentManager, QObject* parent)
    : QObject(parent)
    , m_documentManager(documentManager)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &EmergencyManager::onDocumentChanged);
}

void EmergencyManager::onDocumentChanged(const DocumentChange& change)
{
    if (change.action == ChangeAction::Full)
    {
        syncFromDocument();
    }
    else if (m_response.has_value())
    {
        syncFamilies();
    }
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
        // Update document with final response data before snapshotting
        m_documentManager->setEmergencyResponse(m_response);

        if (!saveArchive())
        {
            // Archive save failed — do NOT clear response data
            // User is notified by saveArchive() and can retry
            return;
        }
    }

    // Clear response data from main document
    m_response.reset();
    m_savedResponse.reset();
    m_documentManager->setEmergencyResponse(std::nullopt);
    emit emergencyEnded();
    emit responseDataChanged();
}

// ============================================================================
// Archive viewing
// ============================================================================

bool EmergencyManager::isViewingArchive() const
{
    return m_documentManager->isViewingArchive();
}

bool EmergencyManager::loadArchive(const QString& filePath, QString* errorMessage)
{
    // Close any existing archive view first
    if (isViewingArchive())
    {
        closeArchive();
    }

    JsonResult result = JsonService::loadDocument(filePath);
    if (!result.success)
    {
        if (errorMessage)
        {
            *errorMessage = tr("Could not load archive:\n%1").arg(result.errorMessage);
        }
        return false;
    }

    const std::optional<EmergencyResponse>& archiveResponse = result.document.emergencyResponse();
    if (!archiveResponse.has_value())
    {
        if (errorMessage)
        {
            *errorMessage = tr("This file does not contain emergency response data.");
        }
        return false;
    }

    // Stash current live response (if any) and swap in archive data
    m_savedResponse = m_response;
    m_response = archiveResponse;
    m_documentManager->setArchiveDocument(std::move(result.document));

    emit archiveViewOpened();
    emit responseDataChanged();
    return true;
}

bool EmergencyManager::reopenArchive(const QString& filePath, QString* errorMessage)
{
    // Close any archive view first
    if (isViewingArchive())
    {
        closeArchive();
    }

    JsonResult result = JsonService::loadDocument(filePath);
    if (!result.success)
    {
        if (errorMessage)
        {
            *errorMessage = tr("Could not load archive:\n%1").arg(result.errorMessage);
        }
        return false;
    }

    const std::optional<EmergencyResponse>& archiveResponse = result.document.emergencyResponse();
    if (!archiveResponse.has_value())
    {
        if (errorMessage)
        {
            *errorMessage = tr("This file does not contain emergency response data.");
        }
        return false;
    }

    // Set the archived response as the active emergency
    m_response = archiveResponse;
    m_response->clearEndedAt();

    // Sync family records with current document (archive may have stale family data)
    syncFamilies();

    persistResponseData();

    // Delete the archive file — it's now the active response
    QFile::remove(filePath);

    emit emergencyStarted();
    emit responseDataChanged();
    return true;
}

void EmergencyManager::closeArchive()
{
    if (!isViewingArchive())
    {
        return;
    }

    m_response = m_savedResponse;
    m_savedResponse.reset();
    m_documentManager->clearArchiveDocument();

    emit archiveViewClosed();
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
    ResponseTask* task = record->mutableTask(taskId);
    if (!task)
    {
        return;
    }
    task->resolve(notes);
    persistResponseData();
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
    persistResponseData();
    emit responseDataChanged();
}

void EmergencyManager::assignTaskToPerson(const FamilyId& familyId, const TaskId& taskId, const PersonId& personId, const QString& notes)
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
    task->assignToPerson(personId, notes);
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
    ResponseTask* task = record->mutableTask(taskId);
    if (!task)
    {
        return;
    }
    task->clearAssignment();
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
    ResponseTask* task = record->mutableTask(taskId);
    if (!task)
    {
        return;
    }
    task->setNotification(notification);
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

bool EmergencyManager::saveArchive()
{
    QString docPath = m_documentManager->filePath();
    if (docPath.isEmpty())
    {
        QMessageBox::critical(
            nullptr,
            tr("Archive Failed"),
            tr("Cannot archive: document has not been saved yet."));
        return false;
    }

    QFileInfo docInfo(docPath);
    QString archiveDir = docInfo.absolutePath() + "/" + docInfo.completeBaseName() + "_archives";

    QDir().mkpath(archiveDir);

    QString slug = m_response->name().toLower().replace(QRegularExpression("[^a-z0-9]+"), "-");
    QString date = m_response->startedAt().toString("yyyy-MM-dd");
    QString archivePath = archiveDir + "/" + date + "-" + slug + ".emergencyplan";

    // Avoid overwriting existing archive — append counter if needed
    if (QFile::exists(archivePath))
    {
        int counter = 2;
        QString basePath = archiveDir + "/" + date + "-" + slug;
        while (QFile::exists(basePath + "-" + QString::number(counter) + ".emergencyplan"))
        {
            ++counter;
        }
        archivePath = basePath + "-" + QString::number(counter) + ".emergencyplan";
    }

    // Snapshot the entire current document (prep + response)
    QString errorMessage;
    bool ok = JsonService::saveDocument(archivePath, m_documentManager->document(), &errorMessage);
    if (!ok)
    {
        qWarning() << "Archive save failed:" << errorMessage;
        QMessageBox::critical(
            nullptr,
            tr("Archive Failed"),
            tr("Could not save emergency archive:\n%1\n\nResponse data has been preserved.")
                .arg(errorMessage));
        return false;
    }
    return true;
}

void EmergencyManager::syncFromDocument()
{
    const std::optional<EmergencyResponse>& response = m_documentManager->document().emergencyResponse();
    if (response.has_value())
    {
        bool wasActive = m_response.has_value();
        m_response = response;
        syncFamilies();
        if (!wasActive)
        {
            emit emergencyStarted();
        }
        emit responseDataChanged();
    }
    else if (m_response.has_value())
    {
        m_response.reset();
        emit emergencyEnded();
        emit responseDataChanged();
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
    bool added = false;
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
            added = true;
        }
    }
    // Removed families: leave their records (snapshot data useful for current emergency)

    if (added)
    {
        persistResponseData();
    }
}
