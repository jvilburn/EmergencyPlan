#include "DocumentManager.h"
#include "GeocodingService.h"
#include "UnitLookupService.h"

DocumentManager* DocumentManager::s_instance = nullptr;

DocumentManager::DocumentManager(const Identity* identity, FileWardStore* store, Transport* transport,
                                 QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_store(store)
    , m_session(identity, store, new GeocodingService(this), new UnitLookupService(this), this)
    , m_server(transport)
    , m_sync(&m_session, &m_server, this)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    connect(&m_session, &WardSession::documentChanged, this, &DocumentManager::documentChanged);
    connect(&m_session, &WardSession::undoStackChanged, this, &DocumentManager::undoStackChanged);
    connect(&m_session, &WardSession::saveFailed, this, &DocumentManager::saveFailed);
    connect(&m_sync, &WardSync::versionFetched, this, &DocumentManager::versionFetched);
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

void DocumentManager::setUseServer(bool use)
{
    m_useServer = use;
    if (use)
    {
        startSync();
    }
    else
    {
        m_sync.stop();
    }
}

bool DocumentManager::retryPending()
{
    if (!m_session.retryPendingSave())
    {
        return false;
    }
    m_sync.retry();
    return true;
}

TakeResult DocumentManager::takeUpdate(const QByteArray& file)
{
    TakeResult result = m_session.takeUpdate(file);
    m_sync.look();
    return result;
}

bool DocumentManager::holdsWardAsItsOwn(const DocumentId& document) const
{
    LocalRead local = WardSession::readLocal(*m_store, *m_identity, document);
    return local.result.status == OpenStatus::Opened && !local.payload->device->noLongerShared;
}

void DocumentManager::startSync()
{
    if (m_useServer && !m_session.documentId().isNull())
    {
        m_sync.start();
    }
}

void DocumentManager::createWard(const UserDetails& user, const QString& deviceName)
{
    m_sync.stop();
    m_session.createWard(Document::empty(), user, deviceName);
    m_session.automaticUpdates().run();
    startSync();
}

OpenResult DocumentManager::openWard(const DocumentId& document)
{
    m_sync.stop();
    OpenResult result = m_session.openWard(document);
    if (result.status == OpenStatus::Opened && !m_session.isNoLongerShared())
    {
        m_session.automaticUpdates().run();
    }
    // A ward that did not open leaves the one before it open, and its sharing goes on.
    startSync();
    return result;
}

void DocumentManager::join(const InvitedWard& invited, const UserDetails& user, const QString& deviceName)
{
    m_sync.stop();
    m_session.join(invited, user, deviceName);
    m_session.automaticUpdates().run();
    startSync();
}
