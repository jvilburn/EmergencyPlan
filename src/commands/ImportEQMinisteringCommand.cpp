#include "ImportEQMinisteringCommand.h"
#include "Document.h"

ImportEQMinisteringCommand::ImportEQMinisteringCommand(
    const QHash<QString, MinisteringDistrict>& districts,
    const QHash<QString, MinisteringGroup>& groups,
    const QHash<QString, Family>& families,
    const QString& description)
    : m_newDistricts(districts)
    , m_newGroups(groups)
    , m_newFamilies(families)
    , m_description(description)
{
}

void ImportEQMinisteringCommand::execute(Document& document)
{
    // Save previous state for undo
    m_previousDistricts = document.eqDistricts();
    m_previousGroups = document.eqGroups();
    m_previousFamilies = document.families();

    // Apply new ministering data
    document.setEqDistricts(m_newDistricts);
    document.setEqGroups(m_newGroups);

    // Set all families (merged: existing + updated + new)
    document.setFamilies(m_newFamilies);
}

void ImportEQMinisteringCommand::undo(Document& document)
{
    // Restore previous state
    document.setEqDistricts(m_previousDistricts);
    document.setEqGroups(m_previousGroups);
    document.setFamilies(m_previousFamilies);
}

QString ImportEQMinisteringCommand::description() const
{
    return m_description;
}
