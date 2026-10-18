#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include <optional>
#include "Document.h"
#include "DocumentChange.h"
#include "WardSession.h"

class UnitLookupService;
class BackgroundGeocodingService;

/// The app's one open ward: a thin wrapper over its WardSession, with the ward's file.
class DocumentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStackChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStackChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)

public:
    explicit DocumentManager(QObject* parent);

    /// Global accessor. Valid after construction.
    static DocumentManager* instance();

    // Document access
    const Document& document() const;

    // Archive document overlay (read-only viewing)
    void setArchiveDocument(Document archiveDoc);
    void clearArchiveDocument();
    bool isViewingArchive() const { return m_archiveDocument.has_value(); }

    // File path
    QString filePath() const { return m_filePath; }

    /// Runs the command through the session's commit step and saves when it applied. A
    /// command that conflicted or was refused emits commandNotApplied.
    CommitResult executeCommand(CommandPtr command);

    /// Undo and redo through the session's commit step, saving when they applied. One that
    /// conflicted or was refused emits commandNotApplied with the undo or redo heading.
    void undo();
    void redo();
    bool canUndo() const { return m_session.canUndo(); }
    bool canRedo() const { return m_session.canRedo(); }
    QString undoHeading() const { return m_session.undoHeading(); }
    QString redoHeading() const { return m_session.redoHeading(); }

    // File operations
    void newDocument();
    bool openDocument(const QString& filePath, QString* errorMessage);
    bool saveDocument(QString* errorMessage);
    bool saveDocumentAs(const QString& filePath, QString* errorMessage);

    // Emergency response data (saves without going through the session)
    void setEmergencyResponse(const std::optional<EmergencyResponse>& response);
    bool saveDocumentOnly();

    // Geocoding
    void startBatchGeocoding();
    void stopGeocoding();
    bool isGeocoding() const;

signals:
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();
    void commandNotApplied(const CommandDescription& description, const CommitResult& result);
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
    /// After the commit step: an applied change looks up new wards and saves; one that
    /// conflicted or was refused emits commandNotApplied with description.
    void afterCommit(const CommitResult& result, const CommandDescription& description);

    void setFilePath(const QString& filePath);
    void checkForIncompleteWards();
    void autoSave();
    void ensureFilePath();
    void maybeRenameForWard();

    WardSession m_session;
    std::optional<Document> m_archiveDocument;  // overlay for read-only archive viewing
    QString m_filePath;

    // Unit lookup
    UnitLookupService* m_unitLookupService;
    QSet<QString> m_pendingWardLookups;
    QSet<QString> m_pendingStakeLookups;

    // Geocoding
    BackgroundGeocodingService* m_geocodingService;

    static DocumentManager* s_instance;
};
