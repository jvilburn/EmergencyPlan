#include "DocumentManager.h"
#include "NominatimGeocoder.h"
#include "UnitLookupService.h"

DocumentManager* DocumentManager::s_instance = nullptr;

DocumentManager::DocumentManager(QObject* parent)
    : QObject(parent)
    , m_session(new NominatimGeocoder(this), new UnitLookupService(this), this)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_session.open(Document::empty());

    connect(&m_session, &WardSession::documentChanged, this, &DocumentManager::documentChanged);
    connect(&m_session, &WardSession::undoStackChanged, this, &DocumentManager::undoStackChanged);
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

void DocumentManager::newDocument()
{
    m_session.open(Document::empty());
    m_session.automaticUpdates().run();
}
