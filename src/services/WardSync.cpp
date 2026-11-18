#include "WardSync.h"

namespace
{
/// The head of a ward the server does not have: no version.
const VersionRank NoHead;
}  // namespace

WardSync::WardSync(WardSession* session, Server* server, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_server(server)
{
    m_retryTimer.setInterval(RetryMilliseconds);
    connect(&m_retryTimer, &QTimer::timeout, this, &WardSync::onRetryTimer);
    connect(m_session, &WardSession::localChange, this, &WardSync::onLocalChange);
}

void WardSync::start()
{
    if (m_running || m_session->isNoLongerShared())
    {
        return;
    }
    m_running = true;
    m_retryTimer.start();
    openChannel();
    look();
}

void WardSync::stop()
{
    m_running = false;
    m_retryTimer.stop();
    delete m_call;
    m_call = nullptr;
    delete m_inviteCall;
    m_inviteCall = nullptr;
    closeChannel();
    m_writing.reset();
    m_unstoredOver.reset();
    m_looking = false;
    m_lookAsked = false;
    m_unjournalledChange = false;
    m_problem = SyncProblem::None;
    emit stateChanged();
    if (m_inviteToConfirm.has_value())
    {
        QByteArray invite = *m_inviteToConfirm;
        m_inviteToConfirm.reset();
        emit inviteConfirmed(invite, false);
    }
}

void WardSync::retry()
{
    if (!m_running)
    {
        return;
    }
    if (m_channel == nullptr)
    {
        openChannel();
    }
    look();
}

bool WardSync::isSyncing() const
{
    return m_connected
        && m_problem == SyncProblem::None
        && !m_session->savePending()
        && !m_session->isNoLongerShared();
}

void WardSync::look()
{
    if (!m_running)
    {
        return;
    }
    if (m_session->isNoLongerShared())
    {
        stop();
        return;
    }
    if (m_looking)
    {
        m_lookAsked = true;
        return;
    }
    m_looking = true;
    ServerReading* head = m_server->head(m_session->documentId(), this);
    connect(head, &ServerReading::answered, this, &WardSync::onHead);
    m_call = head;
}

void WardSync::confirmInvite(const QByteArray& invite)
{
    m_inviteToConfirm = invite;
    if (!m_looking)
    {
        startInviteCheck();
    }
}

void WardSync::onHead(const ServerRead& head)
{
    m_call = nullptr;
    switch (head.status)
    {
    case ServerReadStatus::NoAnswer:
        finish(SyncProblem::Unreachable);
        return;
    case ServerReadStatus::Missing:
        if (m_session->isOnServer())
        {
            m_session->becomeNoLongerShared();
            stop();
            return;
        }
        write(NoHead);
        return;
    case ServerReadStatus::Found:
        break;
    }

    if (!m_session->isOnServer())
    {
        m_session->sawOnServer();
        freshenChannel();
    }
    HeaderRead read = WardHeader::parse(head.bytes);
    switch (read.status)
    {
    case HeaderStatus::Read:
        break;
    case HeaderStatus::Newer:
        finish(SyncProblem::NewerFormat);
        return;
    case HeaderStatus::Unreadable:
        qWarning("The server's head could not be read: %s", qPrintable(read.problem));
        finish(SyncProblem::Unreadable);
        return;
    }

    const VersionRank& theirs = read.header->version();
    if (theirs.ranksAbove(m_session->version()))
    {
        ServerReading* fetch = m_server->fetch(m_session->documentId(), this);
        connect(fetch, &ServerReading::answered, this, &WardSync::onFetched);
        m_call = fetch;
        return;
    }
    if (theirs == m_session->version() && !m_unjournalledChange && !m_session->hasChangesToShare())
    {
        finish(SyncProblem::None);
        return;
    }
    write(theirs);
}

void WardSync::onFetched(const ServerRead& version)
{
    m_call = nullptr;
    switch (version.status)
    {
    case ServerReadStatus::NoAnswer:
        finish(SyncProblem::Unreachable);
        return;
    case ServerReadStatus::Missing:
        lookAgain();
        return;
    case ServerReadStatus::Found:
        break;
    }

    // Taking a version replaces the document, so an automatic change made before it is either
    // in that version or gone, and the run the take starts says so again if it finds work.
    bool unjournalledChange = m_unjournalledChange;
    m_unjournalledChange = false;
    TakeResult result = m_session->takeVersion(version.bytes);
    if (result.status != TakeStatus::Taken)
    {
        m_unjournalledChange = m_unjournalledChange || unjournalledChange;
    }
    emit versionFetched(result);
    switch (result.status)
    {
    case TakeStatus::Taken:
        m_unstoredOver.reset();
        freshenChannel();
        lookAgain();
        return;
    case TakeStatus::NotNewer:
        finish(SyncProblem::None);
        return;
    case TakeStatus::NotNamed:
    case TakeStatus::InviteUsedOrWithdrawn:
        stop();
        return;
    case TakeStatus::Newer:
        finish(SyncProblem::NewerFormat);
        return;
    case TakeStatus::OtherWard:
    case TakeStatus::UnknownWriter:
    case TakeStatus::Unreadable:
        qWarning("The server's version was not taken: %s", qPrintable(result.problem));
        finish(SyncProblem::Unreadable);
        return;
    }
}

void WardSync::write(const VersionRank& over)
{
    if (!m_session->retryPendingSave())
    {
        finish(SyncProblem::None);
        return;
    }
    if (m_unstoredOver.has_value() && *m_unstoredOver == over)
    {
        m_unstoredOver.reset();
        finish(SyncProblem::PushFault);
        return;
    }
    m_writing = Writing{m_session->nextVersion(), m_session->version(), over, m_unjournalledChange};
    m_unjournalledChange = false;
    const NextVersion& next = m_writing->next;
    ServerWrite* write = over == NoHead
        ? m_server->create(m_session->documentId(), next.bytes, next.writer, this)
        : m_server->push(m_session->documentId(), next.bytes, next.writer, this);
    connect(write, &ServerWrite::answered, this, &WardSync::onWritten);
    m_call = write;
}

void WardSync::onWritten(bool stored)
{
    m_call = nullptr;
    Writing writing = *m_writing;
    m_writing.reset();
    if (!stored)
    {
        m_unjournalledChange = m_unjournalledChange || writing.unjournalledChange;
        m_unstoredOver = writing.over;
        lookAgain();
        return;
    }
    m_unstoredOver.reset();
    if (!(m_session->version() == writing.base))
    {
        // An update file was taken while the write was on its way, so this device holds
        // another version now. The head is the stored write, and a look ranks the two.
        lookAgain();
        return;
    }
    m_session->holdPushed(writing.next);
    freshenChannel();
    if (m_unjournalledChange || m_session->hasChangesToShare())
    {
        lookAgain();
        return;
    }
    finish(SyncProblem::None);
}

void WardSync::finish(SyncProblem problem)
{
    m_looking = false;
    m_problem = problem;
    if (problem == SyncProblem::Unreachable)
    {
        // Nothing says whether the server stored the last write, so the next look sends again
        // over a head that has not moved.
        m_unstoredOver.reset();
    }
    emit stateChanged();
    if (m_lookAsked)
    {
        m_lookAsked = false;
        look();
        return;
    }
    if (m_inviteToConfirm.has_value())
    {
        startInviteCheck();
    }
}

void WardSync::lookAgain()
{
    m_looking = false;
    m_lookAsked = false;
    look();
}

void WardSync::startInviteCheck()
{
    if (!m_running || m_problem != SyncProblem::None || m_session->savePending())
    {
        QByteArray invite = *m_inviteToConfirm;
        m_inviteToConfirm.reset();
        emit inviteConfirmed(invite, false);
        return;
    }
    delete m_inviteCall;
    ServerReading* fetch = m_server->fetch(m_session->documentId(), this);
    connect(fetch, &ServerReading::answered, this, &WardSync::onInviteVersionFetched);
    m_inviteCall = fetch;
}

void WardSync::onInviteVersionFetched(const ServerRead& version)
{
    m_inviteCall = nullptr;
    QByteArray invite = *m_inviteToConfirm;
    m_inviteToConfirm.reset();
    emit inviteConfirmed(invite, version.status == ServerReadStatus::Found
                                     && m_session->versionNamesInvite(invite, version.bytes));
}

void WardSync::openChannel()
{
    Identity signer = m_session->writer();
    m_channelSigner = signer.fingerprint();
    m_channel = m_server->openChannel(m_session->documentId(), signer, this);
    connect(m_channel, &ServerChannel::told, this, &WardSync::onTold);
    connect(m_channel, &ServerChannel::closed, this, &WardSync::onChannelClosed);
}

void WardSync::freshenChannel()
{
    if (!m_connected || m_channelSigner != m_session->writer().fingerprint())
    {
        closeChannel();
        openChannel();
    }
}

void WardSync::closeChannel()
{
    if (m_channel != nullptr)
    {
        // A message on the channel can be what leads here, so the channel is closed now and
        // deleted once its signal has returned.
        m_channel->close();
        m_channel->disconnect(this);
        m_channel->deleteLater();
        m_channel = nullptr;
    }
    if (!m_present.isEmpty())
    {
        m_present.clear();
        emit presenceChanged();
    }
    setConnected(false);
}

void WardSync::onLocalChange()
{
    if (!m_session->hasChangesToShare())
    {
        // A command leaves an entry in the journal, which says it is to share. An automatic
        // update leaves none.
        m_unjournalledChange = true;
    }
    look();
}

void WardSync::onTold(const QSet<QByteArray>& present, qint64 version)
{
    QSet<QByteArray> others = present;
    others.remove(m_channelSigner);
    if (others != m_present)
    {
        m_present = others;
        emit presenceChanged();
    }
    setConnected(true);
    if (version > m_session->version().number)
    {
        look();
    }
}

void WardSync::onChannelClosed()
{
    closeChannel();
    look();
}

void WardSync::onRetryTimer()
{
    if (!m_connected || m_problem != SyncProblem::None)
    {
        retry();
    }
}

void WardSync::setConnected(bool connected)
{
    if (m_connected != connected)
    {
        m_connected = connected;
        emit stateChanged();
    }
}
