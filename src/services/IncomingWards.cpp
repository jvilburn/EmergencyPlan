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
    m_timer.start();
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
        add(documentId);
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
    m_library.restore(m_server, this, std::bind(&IncomingWards::onFetched, this, _1, _2));
}

void IncomingWards::onStatus(const QString& documentId, const Result<RequestStatus>& status)
{
    // Unreachable, or a ward that has gone: asked again next time. A granted request
    // is fetched as a listed ward; it is dropped when it arrives.
    if (status.ok() && status.value.value() == RequestStatus::Declined)
    {
        drop(documentId);
        emit declined(documentId);
    }
}

void IncomingWards::onFetched(const QString& documentId, DocumentSession::Download outcome)
{
    switch (outcome)
    {
    case DocumentSession::Download::Done:
        drop(documentId);
        emit fetched(documentId, outcome);
        return;
    case DocumentSession::Download::NeedsNewerVersion:
    case DocumentSession::Download::WriteFailed:
        emit fetched(documentId, outcome);
        return;
    case DocumentSession::Download::Unreachable:
    case DocumentSession::Download::AccessLost:
    case DocumentSession::Download::ServerFault:
        // Not yet sealed to this device, or not reached: tried again next time.
        return;
    }
}

void IncomingWards::add(const QString& documentId)
{
    QStringList wards = pending();
    if (!wards.contains(documentId))
    {
        wards.append(documentId);
        m_settings.setRequestedWards(wards);
    }
}

void IncomingWards::drop(const QString& documentId)
{
    QStringList wards = pending();
    if (wards.removeAll(documentId) > 0)
    {
        m_settings.setRequestedWards(wards);
    }
}
