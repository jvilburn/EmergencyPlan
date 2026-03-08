#include "DocumentManager.h"
#include "JsonService.h"
#include "UnitLookupService.h"
#include "BackgroundGeocodingService.h"

#include <QDebug>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>

static QString buildUniquePath(const QString& dir, const QString& basename, const QString& extension)
{
    QString filePath = dir + "/" + basename + extension;
    if (!QFile::exists(filePath))
    {
        return filePath;
    }

    int counter = 2;
    while (QFile::exists(dir + "/" + basename + " (" + QString::number(counter) + ")" + extension))
    {
        counter++;
    }
    return dir + "/" + basename + " (" + QString::number(counter) + ")" + extension;
}

DocumentManager::DocumentManager(QObject* parent)
    : QObject(parent)
    , m_document(Document::empty())
    , m_commandHistory(this)
    , m_unitLookupService(new UnitLookupService(this))
    , m_geocodingService(new BackgroundGeocodingService(this))
{
    connect(&m_commandHistory, &CommandHistory::canUndoChanged, this, &DocumentManager::canUndoChanged);
    connect(&m_commandHistory, &CommandHistory::canRedoChanged, this, &DocumentManager::canRedoChanged);
    connect(&m_commandHistory, &CommandHistory::dirtyChanged, this, &DocumentManager::dirtyChanged);

    connect(m_unitLookupService, &UnitLookupService::wardLookupComplete,
            this, &DocumentManager::onWardLookupComplete);
    connect(m_unitLookupService, &UnitLookupService::stakeLookupComplete,
            this, &DocumentManager::onStakeLookupComplete);
    connect(m_unitLookupService, &UnitLookupService::lookupFailed,
            this, &DocumentManager::onLookupFailed);

    connect(m_geocodingService, &BackgroundGeocodingService::progressUpdated,
            this, &DocumentManager::geocodingProgressChanged);
    connect(m_geocodingService, &BackgroundGeocodingService::familyGeocoded,
            this, &DocumentManager::onFamilyGeocoded);
    connect(m_geocodingService, &BackgroundGeocodingService::finished,
            this, &DocumentManager::onGeocodingFinished);
}

void DocumentManager::executeCommand(CommandPtr command)
{
    DocumentChange change = command->documentChange();

    m_commandHistory.execute(std::move(command), m_document);
    m_document.onDocumentChanged(change);
    emit documentChanged(change);
    checkForIncompleteWards();
    autoSave();
}

void DocumentManager::undo()
{
    if (!canUndo())
    {
        return;
    }
    DocumentChange change = m_commandHistory.undo(m_document);
    m_document.onDocumentChanged(change);
    emit documentChanged(change);
    autoSave();
}

void DocumentManager::redo()
{
    if (!canRedo())
    {
        return;
    }
    DocumentChange change = m_commandHistory.redo(m_document);
    m_document.onDocumentChanged(change);
    emit documentChanged(change);
    autoSave();
}

void DocumentManager::newDocument()
{
    stopGeocoding();
    m_commandHistory.clear();
    m_pendingWardLookups.clear();
    m_pendingStakeLookups.clear();
    setDocument(Document::empty());
    setFilePath(QString());
}

bool DocumentManager::openDocument(const QString& filePath, QString* errorMessage)
{
    JsonResult result = JsonService::loadDocument(filePath);

    if (!result.success)
    {
        if (errorMessage)
        {
            *errorMessage = result.errorMessage;
        }
        return false;
    }

    stopGeocoding();
    m_commandHistory.clear();
    m_pendingWardLookups.clear();
    m_pendingStakeLookups.clear();
    setDocument(result.document);
    setFilePath(filePath);
    checkForIncompleteWards();
    return true;
}

bool DocumentManager::saveDocument(QString* errorMessage)
{
    if (m_filePath.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = tr("No file path set. Use Save As.");
        }
        return false;
    }

    return saveDocumentAs(m_filePath, errorMessage);
}

bool DocumentManager::saveDocumentAs(const QString& filePath, QString* errorMessage)
{
    if (!JsonService::saveDocument(filePath, m_document, errorMessage))
    {
        return false;
    }

    setFilePath(filePath);
    m_commandHistory.markSaved();
    return true;
}

void DocumentManager::setDocument(const Document& document)
{
    m_document = document;
    m_document.onDocumentChanged(DocumentChange::full());
    emit documentChanged(DocumentChange::full());
}

void DocumentManager::setFilePath(const QString& filePath)
{
    if (m_filePath != filePath)
    {
        m_filePath = filePath;
        emit filePathChanged();
    }
}

void DocumentManager::checkForIncompleteWards()
{
    // Find wards that need lookup (no chapel address means incomplete)
    for (const Ward& ward : m_document.wards())
    {
        QString unitNumber = ward.unitNumber();
        if (!ward.chapelAddress().has_value()
            && !m_pendingWardLookups.contains(unitNumber))
        {
            m_pendingWardLookups.insert(unitNumber);
            m_unitLookupService->lookupWard(unitNumber);
            return;  // One at a time to avoid overloading the API
        }
    }
}

void DocumentManager::onWardLookupComplete(const QString& wardUnitNumber, const Ward& wardInfo)
{
    m_pendingWardLookups.remove(wardUnitNumber);

    // Update ward with chapel info
    std::optional<Ward> existing = m_document.metadata().findWardByUnit(wardUnitNumber);
    if (existing.has_value())
    {
        Ward updated = *existing;

        // Merge in the lookup results (don't overwrite existing data)
        if (wardInfo.chapelAddress().has_value())
        {
            updated.setChapelAddress(wardInfo.chapelAddress());
        }
        if (wardInfo.chapelLat().has_value())
        {
            updated.setChapelLat(wardInfo.chapelLat());
        }
        if (wardInfo.chapelLng().has_value())
        {
            updated.setChapelLng(wardInfo.chapelLng());
        }
        if (wardInfo.chapelPhone().has_value())
        {
            updated.setChapelPhone(wardInfo.chapelPhone());
        }
        if (wardInfo.meetingTime().has_value())
        {
            updated.setMeetingTime(wardInfo.meetingTime());
        }
        if (!wardInfo.stakeUnitNumber().isEmpty())
        {
            updated.setStakeUnitNumber(wardInfo.stakeUnitNumber());
        }

        m_document.metadata().updateWard(updated);
        emit documentChanged(DocumentChange::metadata().updated(wardUnitNumber));

        // Trigger stake lookup if we got stake unit number and don't already have it
        QString stakeUnit = wardInfo.stakeUnitNumber();
        if (!stakeUnit.isEmpty()
            && !m_pendingStakeLookups.contains(stakeUnit)
            && !m_document.metadata().findStakeByUnit(stakeUnit).has_value())
        {
            m_pendingStakeLookups.insert(stakeUnit);
            m_unitLookupService->lookupStake(stakeUnit);
        }
    }

    // Process next incomplete ward
    checkForIncompleteWards();
    autoSave();
}

void DocumentManager::onStakeLookupComplete(const QString& stakeUnitNumber, const Stake& stakeInfo)
{
    m_pendingStakeLookups.remove(stakeUnitNumber);

    // Add or update stake
    m_document.metadata().updateStake(stakeInfo);
    emit documentChanged(DocumentChange::metadata().updated(stakeUnitNumber));
    autoSave();
}

void DocumentManager::onLookupFailed(const QString& unitNumber, const QString& error)
{
    qWarning() << "Lookup failed for" << unitNumber << ":" << error;
    m_pendingWardLookups.remove(unitNumber);
    m_pendingStakeLookups.remove(unitNumber);
    // No retry, no checkForIncompleteWards() - will try again on next file open
}

// ============================================================================
// Geocoding
// ============================================================================

void DocumentManager::startBatchGeocoding()
{
    // Queue all families that need geocoding
    for (const auto& family : m_document.families())
    {
        if (!family.address().isEmpty() && !family.isMapped())
        {
            m_geocodingService->queueFamily(family);
        }
    }
}

void DocumentManager::stopGeocoding()
{
    m_geocodingService->stop();
}

bool DocumentManager::isGeocoding() const
{
    return m_geocodingService->isRunning();
}

void DocumentManager::onFamilyGeocoded(const FamilyId& id, double latitude, double longitude)
{
    // Silent update - no undo/redo, no signal (nothing needs to rebuild for coord changes)
    auto existing = m_document.findFamilyById(id);
    if (existing.has_value())
    {
        Family updated = *existing;
        updated.setLocation(latitude, longitude);
        m_document.updateFamily(updated);
    }
}

void DocumentManager::onGeocodingFinished()
{
    emit geocodingFinished();
    autoSave();
}

// ============================================================================
// Auto-Save
// ============================================================================

void DocumentManager::autoSave()
{
    ensureFilePath();
    maybeRenameForWard();

    if (m_filePath.isEmpty())
    {
        return;
    }

    QString errorMessage;
    if (!saveDocument(&errorMessage))
    {
        qWarning() << "Auto-save failed:" << errorMessage;
        emit autoSaveFailed(errorMessage);
    }
}

void DocumentManager::ensureFilePath()
{
    if (!m_filePath.isEmpty())
    {
        return;
    }

    QString filename = m_document.suggestedFilename();
    if (filename.isEmpty())
    {
        filename = tr("Untitled");
    }

    QString documentsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    setFilePath(buildUniquePath(documentsDir, filename, ".emergencyplan"));
}

void DocumentManager::maybeRenameForWard()
{
    if (m_filePath.isEmpty())
    {
        return;
    }

    QString suggestedName = m_document.suggestedFilename();
    if (suggestedName.isEmpty())
    {
        return;
    }

    QFileInfo fileInfo(m_filePath);
    QString currentBaseName = fileInfo.completeBaseName();

    // Only rename if the file is still "Untitled" (or "Untitled (N)")
    if (!currentBaseName.startsWith(tr("Untitled")))
    {
        return;
    }

    QString newPath = buildUniquePath(fileInfo.absolutePath(), suggestedName, ".emergencyplan");

    // Rename the file on disk (if it exists yet)
    if (QFile::exists(m_filePath))
    {
        if (!QFile::rename(m_filePath, newPath))
        {
            return;  // Rename failed — keep the old path, no harm done
        }
    }

    setFilePath(newPath);
}
