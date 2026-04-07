#include "ImportMinisteringCommand.h"
#include "Document.h"

ImportMinisteringCommand::ImportMinisteringCommand(
    MinisteringOrg org,
    const QHash<MinisteringDistrictId, MinisteringDistrict>& districts,
    const QHash<MinisteringGroupId, MinisteringGroup>& groups,
    const QHash<FamilyId, Family>& families,
    std::optional<QDate> pdfDate,
    const QString& description)
    : m_org(org)
    , m_newDistricts(districts)
    , m_newGroups(groups)
    , m_newFamilies(families)
    , m_description(description)
    , m_pdfDate(pdfDate)
{
}

void ImportMinisteringCommand::execute(Document& document)
{
    // Save previous state for undo
    m_previousDistricts = document.districts(m_org);
    m_previousGroups = document.groups(m_org);
    m_previousFamilies = document.families();
    m_previousMinisteringPdfDate = document.ministeringPdfDate();

    // Apply new ministering data
    document.setDistricts(m_org, m_newDistricts);
    document.setGroups(m_org, m_newGroups);

    // Set all families (merged: existing + updated + new)
    document.setFamilies(m_newFamilies);

    // Update ministering PDF date
    if (m_pdfDate.has_value())
    {
        document.setMinisteringPdfDate(m_pdfDate);
    }
}

void ImportMinisteringCommand::undo(Document& document)
{
    // Restore previous state
    document.setDistricts(m_org, m_previousDistricts);
    document.setGroups(m_org, m_previousGroups);
    document.setFamilies(m_previousFamilies);

    // Restore previous ministering PDF date
    document.setMinisteringPdfDate(m_previousMinisteringPdfDate);
}

QString ImportMinisteringCommand::description() const
{
    return m_description;
}

DocumentChange ImportMinisteringCommand::documentChange() const
{
    return DocumentChange::full();
}
