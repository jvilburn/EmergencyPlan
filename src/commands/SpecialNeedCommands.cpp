#include "SpecialNeedCommands.h"
#include "Document.h"

// ============================================================================
// SetSpecialNeedCommand
// ============================================================================

SetSpecialNeedCommand SetSpecialNeedCommand::forPerson(const QString& personId, const QString& note)
{
    return SetSpecialNeedCommand(personId, std::nullopt, note);
}

SetSpecialNeedCommand SetSpecialNeedCommand::forFamily(const QString& familyId, const QString& note)
{
    return SetSpecialNeedCommand(std::nullopt, familyId, note);
}

SetSpecialNeedCommand::SetSpecialNeedCommand(
    std::optional<QString> personId,
    std::optional<QString> familyId,
    const QString& note)
    : m_personId(std::move(personId))
    , m_familyId(std::move(familyId))
    , m_note(note)
{
}

void SetSpecialNeedCommand::execute(Document& document)
{
    // Store previous state for undo
    m_previousNeed = document.findSpecialNeed(m_personId, m_familyId);

    // Create and set the new special need
    SpecialNeed need;
    need.personId = m_personId;
    need.familyId = m_familyId;
    need.note = m_note;
    document.setSpecialNeed(need);
}

void SetSpecialNeedCommand::undo(Document& document)
{
    if (m_previousNeed)
    {
        // Restore previous need
        document.setSpecialNeed(*m_previousNeed);
    }
    else
    {
        // No previous need existed, so clear it
        document.clearSpecialNeed(m_personId, m_familyId);
    }
}

QString SetSpecialNeedCommand::description() const
{
    if (m_personId)
    {
        return QObject::tr("Set Special Need for Person");
    }
    return QObject::tr("Set Special Need for Family");
}

DocumentChange SetSpecialNeedCommand::documentChange() const
{
    QString entityId = m_personId.value_or(m_familyId.value_or(QString()));
    if (m_previousNeed)
    {
        return DocumentChange::specialNeed().updated(entityId);
    }
    return DocumentChange::specialNeed().added(entityId);
}

// ============================================================================
// ClearSpecialNeedCommand
// ============================================================================

ClearSpecialNeedCommand ClearSpecialNeedCommand::forPerson(const QString& personId)
{
    return ClearSpecialNeedCommand(personId, std::nullopt);
}

ClearSpecialNeedCommand ClearSpecialNeedCommand::forFamily(const QString& familyId)
{
    return ClearSpecialNeedCommand(std::nullopt, familyId);
}

ClearSpecialNeedCommand::ClearSpecialNeedCommand(
    std::optional<QString> personId,
    std::optional<QString> familyId)
    : m_personId(std::move(personId))
    , m_familyId(std::move(familyId))
{
}

void ClearSpecialNeedCommand::execute(Document& document)
{
    // Store for undo
    m_removedNeed = document.findSpecialNeed(m_personId, m_familyId);
    document.clearSpecialNeed(m_personId, m_familyId);
}

void ClearSpecialNeedCommand::undo(Document& document)
{
    if (m_removedNeed)
    {
        document.setSpecialNeed(*m_removedNeed);
    }
}

QString ClearSpecialNeedCommand::description() const
{
    if (m_personId)
    {
        return QObject::tr("Clear Special Need for Person");
    }
    return QObject::tr("Clear Special Need for Family");
}

DocumentChange ClearSpecialNeedCommand::documentChange() const
{
    QString entityId = m_personId.value_or(m_familyId.value_or(QString()));
    return DocumentChange::specialNeed().removed(entityId);
}
