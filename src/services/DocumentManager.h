#pragma once

#include <QObject>
#include <QString>
#include "Document.h"
#include "DocumentChange.h"
#include "FileWardStore.h"
#include "Server.h"
#include "Transport.h"
#include "WardList.h"
#include "WardSession.h"
#include "WardSync.h"

/// The app's one open ward: a thin wrapper over its WardSession, which saves every change
/// through this device's identity into the store of ward files, the WardSync that shares the
/// open ward through the server while the server is in use, and the list of the wards the
/// store holds.
class DocumentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStackChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStackChanged)

public:
    /// identity is this device's, store holds its ward files, and transport reaches the Ward
    /// Planning server; it owns none of them.
    DocumentManager(const Identity* identity, FileWardStore* store, Transport* transport, QObject* parent);

    /// Global accessor. Valid after construction.
    static DocumentManager* instance();

    // Document access
    const Document& document() const { return m_session.document(); }
    AutomaticUpdates& automaticUpdates() { return m_session.automaticUpdates(); }

    /// The open ward's session, for the calls this wrapper does not forward.
    WardSession& session() { return m_session; }

    /// The sharing of the open ward through the server, which runs while the server is in use
    /// and a ward is open.
    WardSync& sync() { return m_sync; }

    /// Use the Ward Planning server, or stop using it: the open ward's sharing starts or stops.
    void setUseServer(bool use);

    /// Asks the server for the current version of the ward document, which need not be one
    /// this device holds, as joining through an invitation link does. The reading is a child
    /// of parent.
    ServerReading* fetchWard(const DocumentId& document, QObject* parent) { return m_server.fetch(document, parent); }

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

    /// Creates a ward of Document::empty(), whose access list names a user with user's details
    /// and this device alone, named deviceName, and starts its automatic updates.
    void createWard(const UserDetails& user, const QString& deviceName);

    /// Opens the ward this device holds under document, and when it opens starts its
    /// automatic updates; otherwise the ward open before stays open.
    OpenResult openWard(const DocumentId& document);

    /// Joins the ward invited carries, which readInvitation found Joinable, through the
    /// session, naming this device deviceName and, for a user invite, giving its new user
    /// user's details, and starts its automatic updates.
    void join(const InvitedWard& invited, const UserDetails& user, const QString& deviceName);

    const DocumentId& documentId() const { return m_session.documentId(); }

    /// The folder that holds this device's ward files.
    const QString& wardsFolder() const { return m_store->directory(); }

    /// Removes the open ward's file from the store. A failure says why in problem, as a clause
    /// with no final period.
    bool removeOpenWard(QString* problem) { return m_store->remove(documentId(), problem); }

    /// The open ward as a save writes it.
    WardPayload payload() const { return m_session.payload(); }

    /// Every ward the store holds, as this device reads it.
    QList<WardListEntry> wards() const { return WardList::read(*m_identity, *m_store); }

    /// True when the store holds a file for the ward document.
    bool holdsWard(const DocumentId& document) const { return m_store->documents().contains(document); }

    /// Tries again whatever is pending: a failed save first, then, when nothing is left
    /// unsaved, what the open ward has to share. True when nothing is left unsaved.
    bool retryPending();

    /// The open ward's next version as an update file, through the session, which then holds it.
    QByteArray writeUpdate() { return m_session.writeUpdate(); }

    /// Takes an update file of the open ward through the session. While the ward is shared
    /// through the server, a look follows, which settles the version taken against the
    /// server's.
    TakeResult takeUpdate(const QByteArray& file);

signals:
    /// A save failed after the one before it succeeded; told once until a save succeeds again.
    void saveFailed(const QString& problem);

    void documentChanged(const DocumentChange& change);
    void undoStackChanged();
    void commandNotApplied(const CommitResult& result);

    /// A version fetched from the server was put to the session, which answered result.
    void versionFetched(const TakeResult& result);

    /// The session's geocoding batch: its progress, and its last request answering.
    void geocodingProgressChanged(int completed, int total);
    void geocodingFinished();

private:
    /// After the commit step: a command that conflicted or was refused emits
    /// commandNotApplied.
    void afterCommit(const CommitResult& result);

    /// Starts sharing the open ward when the server is in use and a ward is open.
    void startSync();

    const Identity* m_identity;
    FileWardStore* m_store;
    WardSession m_session;
    Server m_server;
    WardSync m_sync;  ///< declared after the session and the server it works on
    bool m_useServer = false;

    static DocumentManager* s_instance;
};
