#include "EmergencyManager.h"
#include "DocumentManager.h"

EmergencyManager* EmergencyManager::s_instance = nullptr;

namespace
{

const Document& document()
{
    return DocumentManager::instance()->document();
}

}  // namespace

EmergencyManager::EmergencyManager(QObject* parent)
    : QObject(parent)
    , m_active(document().emergencyResponse().has_value())
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &EmergencyManager::onDocumentChanged);
}

EmergencyManager* EmergencyManager::instance()
{
    return s_instance;
}

const EmergencyResponse& EmergencyManager::response() const
{
    Q_ASSERT(isActive());
    return *document().emergencyResponse();
}

EffectiveContactStatus EmergencyManager::familyStatus(const FamilyId& familyId) const
{
    return document().emergencyStatus(familyId);
}

int EmergencyManager::totalFamilies() const
{
    if (!isActive())
    {
        return 0;
    }
    return response().totalFamilies(document().families());
}

int EmergencyManager::countByStatus(EffectiveContactStatus status) const
{
    if (!isActive())
    {
        return 0;
    }
    return response().countByStatus(status, document().families());
}

void EmergencyManager::onDocumentChanged(const DocumentChange& change)
{
    bool wasActive = m_active;
    m_active = document().emergencyResponse().has_value();

    if (m_active && !wasActive)
    {
        emit emergencyStarted();
    }
    if (!m_active && wasActive)
    {
        emit emergencyEnded();
    }

    // The counts take in every directory family, so a change to the families moves them too
    bool countsMoved = change.action == ChangeAction::Full
        || change.emergencyChanged
        || change.emergencyFamilyId.has_value()
        || change.familyId.has_value();
    if (countsMoved && (m_active || wasActive))
    {
        emit responseDataChanged();
    }
}
