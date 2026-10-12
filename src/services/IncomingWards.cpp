#include "IncomingWards.h"

using namespace ServerContract;
using namespace std::placeholders;

IncomingWards::IncomingWards(AppSettings& settings, DocumentLibrary& library, ServerConnection& server, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_library(library)
    , m_server(server)
{
    m_timer.setInterval(CheckMilliseconds);
    connect(&m_timer, &QTimer::timeout, this, &IncomingWards::checkNow);
    settleWatch();
}

void IncomingWards::request(const QString& documentId)
{
    m_server.requestAccess(documentId, this, std::bind(&IncomingWards::onRequested, this, documentId, _1));
}

void IncomingWards::onRequested(const QString& documentId, const Result<Done>& result)
{
    switch (result.error)
    {
    case ServerError::None:
        add(documentId);
        emit requested(documentId);
        return;
    case ServerError::InvalidRequest:
        // Already a member: nothing to ask, only a ward to fetch.
        checkNow();
        return;
    default:
        emit requestFailed(documentId, result.error);
        return;
    }
}

void IncomingWards::checkNow()
{
    for (const QString& documentId : pending())
    {
        m_server.myRequestStatus(documentId, this, std::bind(&IncomingWards::onStatus, this, documentId, _1));
    }
    m_library.restore(m_server,
                      this,
                      std::bind(&IncomingWards::onListed, this, _1),
                      std::bind(&IncomingWards::onFetched, this, _1, _2),
                      std::bind(&IncomingWards::lostAccess, this, _1, _2));
}

void IncomingWards::onListed(const QStringList& onTheirWay)
{
    // The server's list is the whole of what is on its way: a ward it has stopped
    // listing is no longer waited for.
    m_waiting = onTheirWay;
    settleWatch();
}

void IncomingWards::onStatus(const QString& documentId, const Result<RequestStatus>& status)
{
    switch (status.error)
    {
    case ServerError::None:
        break;
    case ServerError::NotFound:
    case ServerError::UnknownDocument:
        // The request, or the ward, is gone: nothing left to wait for.
        drop(documentId);
        return;
    default:
        // Not reached: asked again next time.
        return;
    }

    switch (status.value.value())
    {
    case RequestStatus::Pending:
        return;
    case RequestStatus::Granted:
        // Now a listed ward, which the restore fetches.
        drop(documentId);
        return;
    case RequestStatus::Declined:
        drop(documentId);
        emit declined(documentId);
        return;
    }
}

void IncomingWards::onFetched(const QString& documentId, DocumentSession::Download outcome)
{
    switch (outcome)
    {
    case DocumentSession::Download::Done:
    case DocumentSession::Download::NeedsNewerVersion:
    case DocumentSession::Download::WriteFailed:
        m_waiting.removeAll(documentId);
        emit fetched(documentId, outcome);
        break;
    case DocumentSession::Download::AccessLost:
        m_waiting.removeAll(documentId);
        break;
    case DocumentSession::Download::NotYetSealed:
    case DocumentSession::Download::Unreachable:
    case DocumentSession::Download::ServerFault:
        // Still on its way: checked again next time.
        break;
    }
    settleWatch();
}

void IncomingWards::add(const QString& documentId)
{
    QStringList wards = pending();
    if (!wards.contains(documentId))
    {
        wards.append(documentId);
        m_settings.setRequestedWards(wards);
    }
    settleWatch();
}

void IncomingWards::drop(const QString& documentId)
{
    QStringList wards = pending();
    if (wards.removeAll(documentId) > 0)
    {
        m_settings.setRequestedWards(wards);
    }
    settleWatch();
}

void IncomingWards::settleWatch()
{
    const bool watching = !pending().isEmpty() || !m_waiting.isEmpty();
    if (watching && !m_timer.isActive())
    {
        m_timer.start();
    }
    else if (!watching)
    {
        m_timer.stop();
    }
}
