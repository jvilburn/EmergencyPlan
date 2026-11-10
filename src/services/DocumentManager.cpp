#include "DocumentManager.h"
#include "GeocodingService.h"
#include "UnitLookupService.h"

DocumentManager* DocumentManager::s_instance = nullptr;

DocumentManager::DocumentManager(const Identity* identity, FileWardStore* store, QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_store(store)
    , m_session(identity, store, new GeocodingService(this), new UnitLookupService(this), this)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    connect(&m_session, &WardSession::documentChanged, this, &DocumentManager::documentChanged);
    connect(&m_session, &WardSession::undoStackChanged, this, &DocumentManager::undoStackChanged);
    connect(&m_session, &WardSession::saveFailed, this, &DocumentManager::saveFailed);
    connect(&m_session.automaticUpdates(), &AutomaticUpdates::geocodingProgress, this, &DocumentManager::geocodingProgressChanged);
    connect(&m_session.automaticUpdates(), &AutomaticUpdates::geocodingFinished, this, &DocumentManager::geocodingFinished);
}

DocumentManager* DocumentManager::instance()
{
    return s_instance;
}

CommitResult DocumentManager::executeCommand(CommandPtr command)
{
    CommitResult result = m_session.commit(command);
    afterCommit(result);
    return result;
}

void DocumentManager::undo()
{
    if (!canUndo())
    {
        return;
    }
    afterCommit(m_session.undo());
}

void DocumentManager::redo()
{
    if (!canRedo())
    {
        return;
    }
    afterCommit(m_session.redo());
}

void DocumentManager::afterCommit(const CommitResult& result)
{
    switch (result.status)
    {
    case CommitStatus::Conflicted:
    case CommitStatus::Refused:
        emit commandNotApplied(result);
        break;
    case CommitStatus::Applied:
    case CommitStatus::NothingToDo:
        break;
    }
}

EndResult DocumentManager::endEmergency(const QString& reportPath)
{
    return m_session.endEmergency(reportPath);
}

void DocumentManager::createWard(const QString& userName, const QString& deviceName)
{
    m_session.createWard(Document::empty(), userName, deviceName);
    m_session.automaticUpdates().run();
}

OpenResult DocumentManager::openWard(const DocumentId& document)
{
    OpenResult result = m_session.openWard(document);
    if (result.status == OpenStatus::Opened)
    {
        m_session.automaticUpdates().run();
    }
    return result;
}
