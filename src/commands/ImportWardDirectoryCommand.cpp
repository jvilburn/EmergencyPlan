#include "ImportWardDirectoryCommand.h"
#include "Document.h"

ImportWardDirectoryCommand::ImportWardDirectoryCommand(
    const QHash<QString, Family>& mergedFamilies,
    const QSet<QString>& removedFamilyIds,
    const QString& wardUnitNumber,
    const QString& wardName,
    std::optional<QDate> pdfDate,
    const QString& description)
    : m_newFamilies(mergedFamilies)
    , m_removedFamilyIds(removedFamilyIds)
    , m_wardUnitNumber(wardUnitNumber)
    , m_wardName(wardName)
    , m_pdfDate(pdfDate)
    , m_description(description)
{
}

void ImportWardDirectoryCommand::execute(Document& document)
{
    // Save previous families for undo
    m_previousFamilies = document.families();

    // Save removed families for undo
    for (const QString& familyId : m_removedFamilyIds)
    {
        auto it = m_previousFamilies.find(familyId);
        if (it != m_previousFamilies.end())
        {
            m_removedFamilies.insert(familyId, *it);
        }
    }

    // Set new families
    document.setFamilies(m_newFamilies);

    // Update ward directory PDF date
    if (m_pdfDate.has_value())
    {
        m_previousWardDirectoryPdfDate = document.wardDirectoryPdfDate();
        document.setWardDirectoryPdfDate(m_pdfDate);
    }

    // Handle ward creation/update
    if (!m_wardUnitNumber.isEmpty())
    {
        std::optional<Ward> existingWard = document.metadata().findWardByUnit(m_wardUnitNumber);
        m_wardExisted = existingWard.has_value();

        if (m_wardExisted)
        {
            // Save previous ward state for undo
            m_previousWard = *existingWard;
        }

        // Create or update ward
        Ward ward;
        if (m_wardExisted)
        {
            // Preserve existing ward data, just ensure it exists
            ward = m_previousWard;
        }
        else
        {
            ward.setUnitNumber(m_wardUnitNumber);
            ward.setName(m_wardName);
        }
        document.metadata().updateWard(ward);
    }
}

void ImportWardDirectoryCommand::undo(Document& document)
{
    // Restore previous families
    document.setFamilies(m_previousFamilies);

    // Restore ward directory PDF date
    if (m_pdfDate.has_value())
    {
        document.setWardDirectoryPdfDate(m_previousWardDirectoryPdfDate);
    }

    // Handle ward removal/restoration
    if (!m_wardUnitNumber.isEmpty())
    {
        if (m_wardExisted)
        {
            // Restore previous ward state
            document.metadata().updateWard(m_previousWard);
        }
        else
        {
            // Ward didn't exist before, remove it
            document.metadata().removeWard(m_wardUnitNumber);
        }
    }
}

QString ImportWardDirectoryCommand::description() const
{
    if (!m_description.isEmpty())
    {
        return m_description;
    }
    return QObject::tr("Import %1 families").arg(m_newFamilies.size());
}
