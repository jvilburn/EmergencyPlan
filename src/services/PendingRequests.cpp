#include "PendingRequests.h"

using namespace ServerContract;
using namespace std::placeholders;

PendingRequests::PendingRequests(AppSettings& settings,
                                 DocumentLibrary& library,
                                 ServerConnection& server,
                                 QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_library(library)
    , m_server(server)
{
    m_timer.setInterval(CheckMilliseconds);
    connect(&m_timer, &QTimer::timeout, this, &PendingRequests::checkNow);
    if (!pending().isEmpty())
    {
        m_timer.start();
    }
}

void PendingRequests::request(const QString& documentId)
{
    m_server.requestAccess(documentId, this, std::bind(&PendingRequests::onRequested, this, documentId, _1));
}

void PendingRequests::onRequested(const QString& documentId, const Result<Done>& result)
{
    switch (result.error)
    {
    case ServerError::None:
        add(documentId);
        emit requested(documentId);
        return;
    case ServerError::InvalidRequest:
        // Already a member: nothing to ask, only a ward to fetch.
        add(documentId);
        fetch(documentId);
        return;
    default:
        emit requestFailed(documentId, result.error);
        return;
    }
}

void PendingRequests::checkNow()
{
    for (const QString& documentId : pending())
    {
        m_server.myRequestStatus(documentId, this, std::bind(&PendingRequests::onStatus, this, documentId, _1));
    }
}

void PendingRequests::onStatus(const QString& documentId, const Result<RequestStatus>& status)
{
    if (!status.ok())
    {
        // Unreachable, or a ward that has gone: asked again next time.
        return;
    }

    switch (status.value.value())
    {
    case RequestStatus::Pending:
        return;
    case RequestStatus::Granted:
        fetch(documentId);
        return;
    case RequestStatus::Declined:
        drop(documentId);
        emit declined(documentId);
        return;
    }
}

void PendingRequests::fetch(const QString& documentId)
{
    m_library.fetchWard(documentId, m_server, this, std::bind(&PendingRequests::onFetched, this, _1, _2));
}

void PendingRequests::onFetched(const QString& documentId, DocumentSession::Download outcome)
{
    // Not yet sealed to this device, or unreachable: tried again next time.
    if (outcome != DocumentSession::Download::Done)
    {
        return;
    }
    drop(documentId);
    emit arrived(documentId);
}

void PendingRequests::add(const QString& documentId)
{
    QStringList wards = pending();
    if (!wards.contains(documentId))
    {
        wards.append(documentId);
        m_settings.setRequestedWards(wards);
    }
    m_timer.start();
}

void PendingRequests::drop(const QString& documentId)
{
    QStringList wards = pending();
    wards.removeAll(documentId);
    m_settings.setRequestedWards(wards);
    if (wards.isEmpty())
    {
        m_timer.stop();
    }
}
