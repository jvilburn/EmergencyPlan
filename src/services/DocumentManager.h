#pragma once

#include <QObject>
#include <QString>
#include "Document.h"
#include "DocumentChange.h"
#include "FileWardStore.h"
#include "WardList.h"
#include "WardSession.h"

/// The app's one open ward: a thin wrapper over its WardSession, which saves every change
/// through this device's identity into the store of ward files, and the list of the wards
/// that store holds.
class DocumentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStackChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStackChanged)

public:
    /// identity is this device's, and store holds its ward files; it owns neither.
    DocumentManager(const Identity* identity, FileWardStore* store, QObject* parent);

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

    /// Writes the report of the emergency in progress and ends it, through the session.
    EndResult endEmergency(const QString& reportPath);

    /// Creates a ward of Document::empty(), whose access list names userName with this
    /// device alone, named deviceName, and starts its automatic updates.
    void createWard(const QString& userName, const QString& deviceName);

    /// Opens the ward this device holds under document, and when it opens starts its
    /// automatic updates; otherwise the ward open before stays open.
    OpenResult openWard(const DocumentId& document);

    const DocumentId& documentId() const { return m_session.documentId(); }

    /// The open ward's file.
    QString filePath() const { return m_store->pathOf(documentId()); }

    /// The open ward as a save writes it.
    WardPayload payload() const { return m_session.payload(); }

    /// Every ward the store holds, as this device reads it.
    QList<WardListEntry> wards() const { return WardList::read(*m_identity, *m_store); }

    /// True when the store holds a file for the ward document.
    bool holdsWard(const DocumentId& document) const { return m_store->documents().contains(document); }

    /// Saves again when a save is pending; true when nothing is left unsaved.
    bool retryPendingSave() { return m_session.retryPendingSave(); }

    /// The open ward's next version as an update file, through the session, which then holds it.
    QByteArray writeUpdate() { return m_session.writeUpdate(); }

    /// Takes an update file of the open ward through the session.
    TakeResult takeUpdate(const QByteArray& file) { return m_session.takeUpdate(file); }

signals:
    /// A save failed after the one before it succeeded; told once until a save succeeds again.
    void saveFailed(const QString& problem);

    void documentChanged(const DocumentChange& change);
    void undoStackChanged();
    void commandNotApplied(const CommitResult& result);

    /// The session's geocoding batch: its progress, and its last request answering.
    void geocodingProgressChanged(int completed, int total);
    void geocodingFinished();

private:
    /// After the commit step: a command that conflicted or was refused emits
    /// commandNotApplied.
    void afterCommit(const CommitResult& result);

    const Identity* m_identity;
    FileWardStore* m_store;
    WardSession m_session;

    static DocumentManager* s_instance;
};
