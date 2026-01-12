#include "ImportRSMinisteringCommand.h"
#include "Document.h"

ImportRSMinisteringCommand::ImportRSMinisteringCommand(
    const QHash<QString, MinisteringDistrict>& districts,
    const QHash<QString, MinisteringGroup>& groups,
    const QHash<QString, Family>& families,
    std::optional<QDate> pdfDate,
    const QString& description)
    : m_newDistricts(districts)
    , m_newGroups(groups)
    , m_newFamilies(families)
    , m_description(description)
    , m_pdfDate(pdfDate)
{
}

void ImportRSMinisteringCommand::execute(Document& document)
{
    // Save previous state for undo
    m_previousDistricts = document.rsDistricts();
    m_previousGroups = document.rsGroups();
    m_previousFamilies = document.families();
    m_previousMinisteringPdfDate = document.ministeringPdfDate();

    // Apply new ministering data
    document.setRsDistricts(m_newDistricts);
    document.setRsGroups(m_newGroups);

    // Set all families (merged: existing + updated + new)
    document.setFamilies(m_newFamilies);

    // Update ministering PDF date
    if (m_pdfDate.has_value())
    {
        document.setMinisteringPdfDate(m_pdfDate);
    }
}

void ImportRSMinisteringCommand::undo(Document& document)
{
    // Restore previous state
    document.setRsDistricts(m_previousDistricts);
    document.setRsGroups(m_previousGroups);
    document.setFamilies(m_previousFamilies);

    // Restore previous ministering PDF date
    document.setMinisteringPdfDate(m_previousMinisteringPdfDate);
}

QString ImportRSMinisteringCommand::description() const
{
    return m_description;
}

DocumentChange ImportRSMinisteringCommand::documentChange() const
{
    // RS ministering import touches districts, groups, and families - use full
    return DocumentChange::full();
}
