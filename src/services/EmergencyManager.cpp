#include "EmergencyManager.h"
#include "DocumentManager.h"

#include <algorithm>

EmergencyManager* EmergencyManager::s_instance = nullptr;

EmergencyManager::EmergencyManager(QObject* parent)
    : QObject(parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &EmergencyManager::onDocumentChanged);
    showCurrent();
}

EmergencyManager* EmergencyManager::instance()
{
    return s_instance;
}

bool EmergencyManager::isActive() const
{
    return m_shownId.has_value()
        && DocumentManager::instance()->document().emergencyResponses().contains(*m_shownId);
}

const EmergencyResponse& EmergencyManager::response() const
{
    Q_ASSERT(isActive());
    const QHash<EmergencyResponseId, EmergencyResponse>& responses = DocumentManager::instance()->document().emergencyResponses();
    return *responses.constFind(*m_shownId);
}

QList<EmergencyResponse> EmergencyManager::responsesInProgress() const
{
    QList<EmergencyResponse> responses = DocumentManager::instance()->document().emergencyResponses().values();
    std::sort(responses.begin(), responses.end(),
              [](const EmergencyResponse& a, const EmergencyResponse& b) { return a.startedAt() < b.startedAt(); });
    return responses;
}

void EmergencyManager::show(const EmergencyResponseId& id)
{
    if (m_shownId == id)
    {
        return;
    }
    m_shownId = id;
    emit emergencyStarted();
    emit responseDataChanged();
}

EffectiveContactStatus EmergencyManager::familyStatus(const FamilyId& familyId) const
{
    if (!m_shownId.has_value())
    {
        return EffectiveContactStatus::NotContacted;
    }
    return DocumentManager::instance()->document().emergencyStatus(*m_shownId, familyId);
}

int EmergencyManager::totalFamilies() const
{
    if (!isActive())
    {
        return 0;
    }
    return response().totalFamilies(DocumentManager::instance()->document().families());
}

int EmergencyManager::countByStatus(EffectiveContactStatus status) const
{
    if (!isActive())
    {
        return 0;
    }
    return response().countByStatus(status, DocumentManager::instance()->document().families());
}

void EmergencyManager::onDocumentChanged(const DocumentChange& change)
{
    if (change.action == ChangeAction::Full)
    {
        emit responsesChanged();
        if (!showCurrent())
        {
            emit responseDataChanged();
        }
        return;
    }
    if (!change.responseId.has_value())
    {
        return;
    }

    if (!change.responseFamilyId.has_value())
    {
        emit responsesChanged();
    }
    if (m_shownId != change.responseId || !isActive())
    {
        showCurrent();
        return;
    }
    if (change.responseFamilyId.has_value())
    {
        emit familyStatusChanged(*change.responseFamilyId);
    }
    emit responseDataChanged();
}

bool EmergencyManager::showCurrent()
{
    if (isActive())
    {
        return false;
    }

    std::optional<EmergencyResponseId> previous = m_shownId;
    m_shownId.reset();
    QList<EmergencyResponse> inProgress = responsesInProgress();
    if (!inProgress.isEmpty())
    {
        m_shownId = inProgress.last().id();
    }

    if (m_shownId == previous)
    {
        return false;
    }
    if (m_shownId.has_value())
    {
        emit emergencyStarted();
    }
    else
    {
        emit emergencyEnded();
    }
    emit responseDataChanged();
    return true;
}
