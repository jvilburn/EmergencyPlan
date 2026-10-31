#pragma once

#include <QObject>
#include <QString>
#include "Document.h"
#include "DocumentChange.h"
#include "WardSession.h"

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

    /// Opens Document::empty() in the session and starts its automatic updates.
    void newDocument();

signals:
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

    WardSession m_session;

    static DocumentManager* s_instance;
};
