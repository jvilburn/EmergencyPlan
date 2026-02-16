#include "ImportEQMinisteringCommand.h"
#include "Document.h"

ImportEQMinisteringCommand::ImportEQMinisteringCommand(
    const QHash<MinisteringDistrictId, MinisteringDistrict>& districts,
    const QHash<MinisteringGroupId, MinisteringGroup>& groups,
    const QHash<FamilyId, Family>& families,
    std::optional<QDate> pdfDate,
    const QString& description)
    : m_newDistricts(districts)
    , m_newGroups(groups)
    , m_newFamilies(families)
    , m_description(description)
    , m_pdfDate(pdfDate)
{
}

void ImportEQMinisteringCommand::execute(Document& document)
{
    // Save previous state for undo
    m_previousDistricts = document.eqDistricts();
    m_previousGroups = document.eqGroups();
    m_previousFamilies = document.families();
    m_previousMinisteringPdfDate = document.ministeringPdfDate();

    // Apply new ministering data
    document.setEqDistricts(m_newDistricts);
    document.setEqGroups(m_newGroups);

    // Set all families (merged: existing + updated + new)
    document.setFamilies(m_newFamilies);

    // Update ministering PDF date
    if (m_pdfDate.has_value())
    {
        document.setMinisteringPdfDate(m_pdfDate);
    }
}

void ImportEQMinisteringCommand::undo(Document& document)
{
    // Restore previous state
    document.setEqDistricts(m_previousDistricts);
    document.setEqGroups(m_previousGroups);
    document.setFamilies(m_previousFamilies);

    // Restore previous ministering PDF date
    document.setMinisteringPdfDate(m_previousMinisteringPdfDate);
}

QString ImportEQMinisteringCommand::description() const
{
    return m_description;
}

DocumentChange ImportEQMinisteringCommand::documentChange() const
{
    // EQ ministering import touches districts, groups, and families - use full
    return DocumentChange::full();
}
