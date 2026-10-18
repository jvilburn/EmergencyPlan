#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include "Document.h"
#include "DocumentChange.h"
#include "WardSession.h"

class UnitLookupService;
class BackgroundGeocodingService;

/// The app's one open ward: a thin wrapper over its WardSession.
class DocumentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStackChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStackChanged)

public:
    explicit DocumentManager(QObject* parent);

    /// Global accessor. Valid after construction.
    static DocumentManager* instance();

    // Document access
    const Document& document() const { return m_session.document(); }

    /// Runs the command through the session's commit step. A command that conflicted or was
    /// refused emits commandNotApplied.
    CommitResult executeCommand(CommandPtr command);

    /// Undo and redo through the session's commit step. One that conflicted or was refused
    /// emits commandNotApplied.
    void undo();
    void redo();
    bool canUndo() const { return m_session.canUndo(); }
    bool canRedo() const { return m_session.canRedo(); }
    QString undoHeading() const { return m_session.undoHeading(); }
    QString redoHeading() const { return m_session.redoHeading(); }

    /// Opens Document::empty() in the session.
    void newDocument();

    // Geocoding
    void startBatchGeocoding();
    void stopGeocoding();

signals:
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();
    void commandNotApplied(const CommitResult& result);

    // Geocoding progress
    void geocodingProgressChanged(int completed, int total);
    void geocodingFinished();

private slots:
    void onWardLookupComplete(const QString& wardUnitNumber, const Ward& ward);
    void onStakeLookupComplete(const QString& stakeUnitNumber, const Stake& stake);
    void onLookupFailed(const QString& unitNumber, const QString& error);

    // Geocoding
    void onFamilyGeocoded(const FamilyId& id, double latitude, double longitude);
    void onGeocodingFinished();

private:
    /// After the commit step: an applied change looks up new wards; one that conflicted or
    /// was refused emits commandNotApplied.
    void afterCommit(const CommitResult& result);

    void checkForIncompleteWards();

    WardSession m_session;

    // Unit lookup
    UnitLookupService* m_unitLookupService;
    QSet<QString> m_pendingWardLookups;
    QSet<QString> m_pendingStakeLookups;

    // Geocoding
    BackgroundGeocodingService* m_geocodingService;

    static DocumentManager* s_instance;
};
