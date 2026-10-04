#include "RosterChangeQueue.h"

#include "DocumentFile.h"

#include <algorithm>

using namespace ServerContract;
using namespace std::placeholders;
using ServerVersion::Fetched;

RosterChangeQueue::RosterChangeQueue(const DeviceIdentity& identity,
                                     DocumentRecords& records,
                                     ServerConnection& server,
                                     std::unique_ptr<SessionTimers> timers,
                                     std::function<DocumentPaths(const QString&)> pathsFor,
                                     QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_records(records)
    , m_server(server)
    , m_timers(std::move(timers))
    , m_pathsFor(pathsFor)
{
    m_timers->onTimeout = std::bind(&RosterChangeQueue::onTimer, this, _1);

    // Resumes whatever the records hold.
    processNext();
}

void RosterChangeQueue::setOpenSession(DocumentSession* session)
{
    if (m_openSession)
    {
        disconnect(m_openSession, nullptr, this, nullptr);
    }

    m_openSession = session;
    if (session != nullptr)
    {
        connect(session, &DocumentSession::shared, this, &RosterChangeQueue::onShared);
    }
}

void RosterChangeQueue::enqueue(const QString& documentId, const RosterChange& change)
{
    DocumentRecord record = m_records.record(documentId);

    // Until a ward is shared only this device reads it, so an unlink of a device
    // whose link has not yet joined it simply cancels the link.
    bool cancelled = false;
    if (!record.baseVersion.has_value() && change.kind == RosterChange::Kind::Unlink && change.device.has_value())
    {
        for (qsizetype index = 0; index < record.pendingChanges.size(); ++index)
        {
            const RosterChange& pending = record.pendingChanges.at(index);
            if (pending.kind == RosterChange::Kind::Link && pending.device.has_value()
                && pending.device.value().fingerprint() == change.device.value().fingerprint())
            {
                record.pendingChanges.removeAt(index);
                cancelled = true;
                break;
            }
        }
    }

    if (!cancelled)
    {
        record.pendingChanges.append(change);
    }

    m_records.setRecord(documentId, record);
    emit pendingChanged();
    processNext();
}

void RosterChangeQueue::enqueueForEveryWard(const RosterChange& change, const QStringList& documentIds)
{
    for (const QString& documentId : documentIds)
    {
        enqueue(documentId, change);
    }
}

QList<RosterChange> RosterChangeQueue::pending(const QString& documentId) const
{
    return m_records.record(documentId).pendingChanges;
}

void RosterChangeQueue::shareOpenWard()
{
    if (!m_openSession)
    {
        return;
    }

    QList<RosterDevice> linked;
    for (const RosterChange& change : pending(m_openSession->documentId()))
    {
        if (change.kind == RosterChange::Kind::Link && change.device.has_value())
        {
            linked.append(change.device.value());
        }
    }

    m_openSession->share(linked);
}

void RosterChangeQueue::onShared()
{
    // Each Link either joined the first header or no longer applies to it.
    const QString documentId = m_openSession->documentId();
    const AccessRoster& roster = m_openSession->document().accessRoster();
    for (const RosterChange& change : pending(documentId))
    {
        if (change.kind != RosterChange::Kind::Link || !change.device.has_value())
        {
            continue;
        }

        finish(documentId,
               change,
               roster.deviceFor(change.device.value().fingerprint()).has_value() ? RosterChangeOutcome::Published
                                                                                 : RosterChangeOutcome::NoLongerApplies);
    }

    processNext();
}

void RosterChangeQueue::onTimer(SessionTimers::Timer timer)
{
    if (timer != SessionTimers::Timer::Retry)
    {
        return;
    }

    m_retryWaiting = false;
    m_deferred.clear();
    processNext();
}

void RosterChangeQueue::processNext()
{
    if (m_busy)
    {
        return;
    }

    QStringList documentIds = m_records.documentIds();
    std::sort(documentIds.begin(), documentIds.end());

    for (const QString& documentId : documentIds)
    {
        const DocumentRecord record = m_records.record(documentId);
        if (!record.baseVersion.has_value() || record.pendingChanges.isEmpty() || m_deferred.contains(documentId))
        {
            continue;
        }

        const RosterChange change = record.pendingChanges.first();
        m_busy = true;
        ServerVersion::fetch(m_identity,
                             m_records,
                             m_server,
                             documentId,
                             m_pathsFor(documentId),
                             this,
                             std::bind(&RosterChangeQueue::onFetched, this, documentId, change, _1));
        return;
    }

    // Only wards waiting for the next Retry are left.
    if (!m_deferred.isEmpty() && !m_retryWaiting)
    {
        m_retryWaiting = true;
        m_timers->start(SessionTimers::Timer::Retry, m_retryDelay);
        m_retryDelay = qMin(m_retryDelay * 2, SessionTimers::LastRetryMilliseconds);
    }
}

void RosterChangeQueue::onFetched(const QString& documentId, const RosterChange& change, const Fetched& theirs)
{
    m_busy = false;

    switch (theirs.outcome)
    {
    case Fetched::Outcome::Accepted:
        break;
    case Fetched::Outcome::Removed:
        forgetWard(documentId, theirs.remover, theirs.left);
        return;
    case Fetched::Outcome::NeedsNewerVersion:
    case Fetched::Outcome::CallFailed:
        defer(documentId);
        return;
    case Fetched::Outcome::ServerFault:
        finish(documentId, change, RosterChangeOutcome::ValidationFailed);
        processNext();
        return;
    }

    if (alreadyDone(change, theirs.document.accessRoster()))
    {
        finish(documentId, change, RosterChangeOutcome::AlreadyDone);
        processNext();
        return;
    }

    // Refused against their version - this person is no longer an admin, or is now
    // the last admin trying to leave. Not retried.
    const std::optional<AccessChange> access = apply(change, theirs);
    if (!access.has_value())
    {
        finish(documentId, change, RosterChangeOutcome::NoLongerApplies);
        processNext();
        return;
    }

    Document document = theirs.document;
    document.setAccessRoster(access.value().roster);
    const std::optional<DocumentFile::Sealed> sealed =
        DocumentFile::seal(document, access.value().header, access.value().dataKey, m_identity);
    if (!sealed.has_value())
    {
        finish(documentId, change, RosterChangeOutcome::ValidationFailed);
        processNext();
        return;
    }

    m_busy = true;
    m_server.upload(documentId,
                    theirs.version,
                    Upload{sealed.value().header, sealed.value().ciphertext},
                    this,
                    std::bind(&RosterChangeQueue::onUploaded, this, documentId, change, _1));
}

void RosterChangeQueue::onUploaded(const QString& documentId, const RosterChange& change, const Result<qint64>& result)
{
    m_busy = false;

    switch (result.error)
    {
    case ServerError::None:
        m_retryDelay = SessionTimers::FirstRetryMilliseconds;
        finish(documentId, change, RosterChangeOutcome::Published);

        // A leave, once accepted, is this device off the ward.
        if (change.kind == RosterChange::Kind::Leave)
        {
            forgetWard(documentId, m_identity.fingerprint(), true);
            return;
        }

        emit published(documentId);
        if (m_openSession && m_openSession->documentId() == documentId)
        {
            m_openSession->checkServer();
        }
        processNext();
        return;

    case ServerError::NotAllowListed:
        m_busy = true;
        ServerVersion::proveRemoval(m_identity,
                                    m_records,
                                    m_server,
                                    documentId,
                                    m_pathsFor(documentId),
                                    this,
                                    std::bind(&RosterChangeQueue::onRemovalChecked, this, documentId, change, _1));
        return;

    case ServerError::DeviceNotInvited:
        // The newcomer's request is gone: withdrawn, or granted by someone else.
        finish(documentId, change, RosterChangeOutcome::NoLongerApplies);
        processNext();
        return;

    case ServerError::ChainRefused:
    case ServerError::PayloadMismatch:
    case ServerError::CounterJump:
    case ServerError::InvalidRequest:
        // The server refused the version itself; sending it again changes nothing.
        finish(documentId, change, RosterChangeOutcome::ValidationFailed);
        processNext();
        return;

    default:
        // A conflict, or a call that did not get through: try again later.
        defer(documentId);
        return;
    }
}

void RosterChangeQueue::onRemovalChecked(const QString& documentId, const RosterChange& change, const Fetched& proof)
{
    m_busy = false;

    switch (proof.outcome)
    {
    case Fetched::Outcome::Removed:
        forgetWard(documentId, proof.remover, proof.left);
        return;
    case Fetched::Outcome::CallFailed:
        defer(documentId);
        return;
    case Fetched::Outcome::Accepted:
    case Fetched::Outcome::NeedsNewerVersion:
    case Fetched::Outcome::ServerFault:
        finish(documentId, change, RosterChangeOutcome::ValidationFailed);
        processNext();
        return;
    }
}

bool RosterChangeQueue::alreadyDone(const RosterChange& change, const AccessRoster& roster) const
{
    switch (change.kind)
    {
    case RosterChange::Kind::Grant:
        return change.person.has_value() && roster.contains(change.person.value().member());
    case RosterChange::Kind::Link:
        return change.device.has_value() && roster.deviceFor(change.device.value().fingerprint()).has_value();
    case RosterChange::Kind::Unlink:
        return change.device.has_value() && !roster.deviceFor(change.device.value().fingerprint()).has_value();
    case RosterChange::Kind::Remove:
        return change.target.has_value() && !roster.contains(change.target.value());
    case RosterChange::Kind::SetRole:
    {
        if (!change.target.has_value() || !change.role.has_value())
        {
            return false;
        }
        const std::optional<RosterPerson> person = roster.person(change.target.value());
        return person.has_value() && person.value().role() == change.role.value();
    }
    case RosterChange::Kind::Leave:
        return !roster.contains(m_identity.member);
    }

    return false;
}

std::optional<AccessChange> RosterChangeQueue::apply(const RosterChange& change, const Fetched& theirs) const
{
    const DocumentHeader& header = theirs.sealed.header;
    const AccessRoster& roster = theirs.document.accessRoster();
    const Fingerprint writer = m_identity.fingerprint();

    switch (change.kind)
    {
    case RosterChange::Kind::Grant:
        if (!change.person.has_value())
        {
            return std::nullopt;
        }
        return AccessControl::grant(header, roster, theirs.dataKey, writer, change.person.value());

    case RosterChange::Kind::Link:
        if (!change.device.has_value())
        {
            return std::nullopt;
        }
        return AccessControl::linkDevice(header, roster, theirs.dataKey, writer, change.device.value());

    case RosterChange::Kind::Unlink:
        if (!change.device.has_value())
        {
            return std::nullopt;
        }
        return AccessControl::unlinkDevice(header, roster, writer, change.device.value().fingerprint());

    case RosterChange::Kind::Remove:
        if (!change.target.has_value())
        {
            return std::nullopt;
        }
        return AccessControl::remove(header, roster, writer, change.target.value());

    case RosterChange::Kind::SetRole:
        if (!change.target.has_value() || !change.role.has_value())
        {
            return std::nullopt;
        }
        return AccessControl::setRole(header, roster, theirs.dataKey, writer, change.target.value(), change.role.value());

    case RosterChange::Kind::Leave:
        return AccessControl::leave(header, roster, theirs.dataKey, writer);
    }

    return std::nullopt;
}

void RosterChangeQueue::finish(const QString& documentId, const RosterChange& change, RosterChangeOutcome outcome)
{
    DocumentRecord record = m_records.record(documentId);
    if (record.pendingChanges.removeOne(change))
    {
        m_records.setRecord(documentId, record);
        emit pendingChanged();
    }

    emit changeFinished(documentId, change, outcome);
}

void RosterChangeQueue::forgetWard(const QString& documentId, const Fingerprint& remover, bool left)
{
    const QList<RosterChange> remaining = pending(documentId);

    // An open ward finishes through its session, which then writes nothing more.
    if (m_openSession && m_openSession->documentId() == documentId)
    {
        m_openSession->finishRemoved(remover, left);
    }
    else
    {
        ServerVersion::forget(m_records, documentId, m_pathsFor(documentId));
    }

    // A leave still waiting is how a leave whose answer was lost comes back: this
    // device is off the ward, which is what it asked for.
    for (const RosterChange& change : remaining)
    {
        emit changeFinished(documentId,
                            change,
                            change.kind == RosterChange::Kind::Leave ? RosterChangeOutcome::AlreadyDone
                                                                     : RosterChangeOutcome::NoLongerApplies);
    }

    m_deferred.remove(documentId);
    emit pendingChanged();
    emit removed(documentId, left);
    processNext();
}

void RosterChangeQueue::defer(const QString& documentId)
{
    m_deferred.insert(documentId);
    processNext();
}
