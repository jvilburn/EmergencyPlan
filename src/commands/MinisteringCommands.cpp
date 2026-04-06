#include "MinisteringCommands.h"
#include "Document.h"

// ============================================================================
// EQ District Commands
// ============================================================================

AddEqDistrictCommand::AddEqDistrictCommand(const MinisteringDistrict& district)
    : m_district(district)
{
}

void AddEqDistrictCommand::execute(Document& document)
{
    document.addEqDistrict(m_district);
}

void AddEqDistrictCommand::undo(Document& document)
{
    document.removeEqDistrict(m_district.id());
}

QString AddEqDistrictCommand::description() const
{
    return QObject::tr("Add EQ District \"%1\"").arg(m_district.name());
}

DocumentChange AddEqDistrictCommand::documentChange() const
{
    return DocumentChange::eqDistrict().added(m_district.id());
}

UpdateEqDistrictCommand::UpdateEqDistrictCommand(const MinisteringDistrict& oldDistrict,
                                                  const MinisteringDistrict& newDistrict)
    : m_oldDistrict(oldDistrict)
    , m_newDistrict(newDistrict)
{
}

void UpdateEqDistrictCommand::execute(Document& document)
{
    document.updateEqDistrict(m_newDistrict);
}

void UpdateEqDistrictCommand::undo(Document& document)
{
    document.updateEqDistrict(m_oldDistrict);
}

QString UpdateEqDistrictCommand::description() const
{
    return QObject::tr("Update EQ District \"%1\"").arg(m_oldDistrict.name());
}

DocumentChange UpdateEqDistrictCommand::documentChange() const
{
    return DocumentChange::eqDistrict().updated(m_newDistrict.id());
}

DeleteEqDistrictCommand::DeleteEqDistrictCommand(const MinisteringDistrict& district)
    : m_district(district)
{
}

void DeleteEqDistrictCommand::execute(Document& document)
{
    document.removeEqDistrict(m_district.id());
}

void DeleteEqDistrictCommand::undo(Document& document)
{
    document.addEqDistrict(m_district);
}

QString DeleteEqDistrictCommand::description() const
{
    return QObject::tr("Delete EQ District \"%1\"").arg(m_district.name());
}

DocumentChange DeleteEqDistrictCommand::documentChange() const
{
    return DocumentChange::eqDistrict().removed(m_district.id());
}

// ============================================================================
// EQ Group Commands
// ============================================================================

AddEqGroupCommand::AddEqGroupCommand(const MinisteringGroup& group)
    : m_group(group)
{
}

void AddEqGroupCommand::execute(Document& document)
{
    document.addEqGroup(m_group);
}

void AddEqGroupCommand::undo(Document& document)
{
    document.removeEqGroup(m_group.id());
}

QString AddEqGroupCommand::description() const
{
    return QObject::tr("Add EQ Ministering Group");
}

DocumentChange AddEqGroupCommand::documentChange() const
{
    return DocumentChange::eqGroup().added(m_group.id());
}

UpdateEqGroupCommand::UpdateEqGroupCommand(const MinisteringGroup& oldGroup,
                                            const MinisteringGroup& newGroup)
    : m_oldGroup(oldGroup)
    , m_newGroup(newGroup)
{
}

void UpdateEqGroupCommand::execute(Document& document)
{
    document.updateEqGroup(m_newGroup);
}

void UpdateEqGroupCommand::undo(Document& document)
{
    document.updateEqGroup(m_oldGroup);
}

QString UpdateEqGroupCommand::description() const
{
    return QObject::tr("Update EQ Ministering Group");
}

DocumentChange UpdateEqGroupCommand::documentChange() const
{
    return DocumentChange::eqGroup().updated(m_newGroup.id());
}

DeleteEqGroupCommand::DeleteEqGroupCommand(const MinisteringGroup& group)
    : m_group(group)
{
}

void DeleteEqGroupCommand::execute(Document& document)
{
    document.removeEqGroup(m_group.id());
}

void DeleteEqGroupCommand::undo(Document& document)
{
    document.addEqGroup(m_group);
}

QString DeleteEqGroupCommand::description() const
{
    return QObject::tr("Delete EQ Ministering Group");
}

DocumentChange DeleteEqGroupCommand::documentChange() const
{
    return DocumentChange::eqGroup().removed(m_group.id());
}

// ============================================================================
// RS District Commands
// ============================================================================

AddRsDistrictCommand::AddRsDistrictCommand(const MinisteringDistrict& district)
    : m_district(district)
{
}

void AddRsDistrictCommand::execute(Document& document)
{
    document.addRsDistrict(m_district);
}

void AddRsDistrictCommand::undo(Document& document)
{
    document.removeRsDistrict(m_district.id());
}

QString AddRsDistrictCommand::description() const
{
    return QObject::tr("Add RS District \"%1\"").arg(m_district.name());
}

DocumentChange AddRsDistrictCommand::documentChange() const
{
    return DocumentChange::rsDistrict().added(m_district.id());
}

UpdateRsDistrictCommand::UpdateRsDistrictCommand(const MinisteringDistrict& oldDistrict,
                                                  const MinisteringDistrict& newDistrict)
    : m_oldDistrict(oldDistrict)
    , m_newDistrict(newDistrict)
{
}

void UpdateRsDistrictCommand::execute(Document& document)
{
    document.updateRsDistrict(m_newDistrict);
}

void UpdateRsDistrictCommand::undo(Document& document)
{
    document.updateRsDistrict(m_oldDistrict);
}

QString UpdateRsDistrictCommand::description() const
{
    return QObject::tr("Update RS District \"%1\"").arg(m_oldDistrict.name());
}

DocumentChange UpdateRsDistrictCommand::documentChange() const
{
    return DocumentChange::rsDistrict().updated(m_newDistrict.id());
}

DeleteRsDistrictCommand::DeleteRsDistrictCommand(const MinisteringDistrict& district)
    : m_district(district)
{
}

void DeleteRsDistrictCommand::execute(Document& document)
{
    document.removeRsDistrict(m_district.id());
}

void DeleteRsDistrictCommand::undo(Document& document)
{
    document.addRsDistrict(m_district);
}

QString DeleteRsDistrictCommand::description() const
{
    return QObject::tr("Delete RS District \"%1\"").arg(m_district.name());
}

DocumentChange DeleteRsDistrictCommand::documentChange() const
{
    return DocumentChange::rsDistrict().removed(m_district.id());
}

// ============================================================================
// RS Group Commands
// ============================================================================

AddRsGroupCommand::AddRsGroupCommand(const MinisteringGroup& group)
    : m_group(group)
{
}

void AddRsGroupCommand::execute(Document& document)
{
    document.addRsGroup(m_group);
}

void AddRsGroupCommand::undo(Document& document)
{
    document.removeRsGroup(m_group.id());
}

QString AddRsGroupCommand::description() const
{
    return QObject::tr("Add RS Ministering Group");
}

DocumentChange AddRsGroupCommand::documentChange() const
{
    return DocumentChange::rsGroup().added(m_group.id());
}

UpdateRsGroupCommand::UpdateRsGroupCommand(const MinisteringGroup& oldGroup,
                                            const MinisteringGroup& newGroup)
    : m_oldGroup(oldGroup)
    , m_newGroup(newGroup)
{
}

void UpdateRsGroupCommand::execute(Document& document)
{
    document.updateRsGroup(m_newGroup);
}

void UpdateRsGroupCommand::undo(Document& document)
{
    document.updateRsGroup(m_oldGroup);
}

QString UpdateRsGroupCommand::description() const
{
    return QObject::tr("Update RS Ministering Group");
}

DocumentChange UpdateRsGroupCommand::documentChange() const
{
    return DocumentChange::rsGroup().updated(m_newGroup.id());
}

DeleteRsGroupCommand::DeleteRsGroupCommand(const MinisteringGroup& group)
    : m_group(group)
{
}

void DeleteRsGroupCommand::execute(Document& document)
{
    document.removeRsGroup(m_group.id());
}

void DeleteRsGroupCommand::undo(Document& document)
{
    document.addRsGroup(m_group);
}

QString DeleteRsGroupCommand::description() const
{
    return QObject::tr("Delete RS Ministering Group");
}

DocumentChange DeleteRsGroupCommand::documentChange() const
{
    return DocumentChange::rsGroup().removed(m_group.id());
}
