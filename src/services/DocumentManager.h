#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include "Document.h"
#include "DocumentChange.h"
#include "CommandHistory.h"

class UnitLookupService;
class BackgroundGeocodingService;

class DocumentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isDirty READ isDirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)

public:
    explicit DocumentManager(QObject* parent);

    // Document access
    const Document& document() const { return m_document; }

    // File path
    QString filePath() const { return m_filePath; }

    // Command execution
    void executeCommand(CommandPtr command);

    // Undo/Redo
    void undo();
    void redo();
    bool canUndo() const { return m_commandHistory.canUndo(); }
    bool canRedo() const { return m_commandHistory.canRedo(); }
    QString undoDescription() const { return m_commandHistory.undoDescription(); }
    QString redoDescription() const { return m_commandHistory.redoDescription(); }

    // Dirty state
    bool isDirty() const { return m_commandHistory.isDirty(); }

    // File operations
    void newDocument();
    bool openDocument(const QString& filePath, QString* errorMessage);
    bool saveDocument(QString* errorMessage);
    bool saveDocumentAs(const QString& filePath, QString* errorMessage);

    // Geocoding
    void startBatchGeocoding();
    void stopGeocoding();
    bool isGeocoding() const;

signals:
    void documentChanged(const DocumentChange& change);
    void dirtyChanged();
    void canUndoChanged();
    void canRedoChanged();
    void filePathChanged();

    // Geocoding progress
    void geocodingProgressChanged(int completed, int total);
    void geocodingFinished();

    // Auto-save
    void autoSaveFailed(const QString& errorMessage);

private slots:
    void onWardLookupComplete(const QString& wardUnitNumber, const Ward& ward);
    void onStakeLookupComplete(const QString& stakeUnitNumber, const Stake& stake);
    void onLookupFailed(const QString& unitNumber, const QString& error);

    // Geocoding
    void onFamilyGeocoded(const FamilyId& id, double latitude, double longitude);
    void onGeocodingFinished();

private:
    void setDocument(const Document& document);
    void setFilePath(const QString& filePath);
    void checkForIncompleteWards();
    void autoSave();
    void ensureFilePath();
    void maybeRenameForWard();

    Document m_document;
    CommandHistory m_commandHistory;
    QString m_filePath;

    // Unit lookup
    UnitLookupService* m_unitLookupService;
    QSet<QString> m_pendingWardLookups;
    QSet<QString> m_pendingStakeLookups;

    // Geocoding
    BackgroundGeocodingService* m_geocodingService;
};
