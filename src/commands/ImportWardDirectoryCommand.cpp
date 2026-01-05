#include "ImportWardDirectoryCommand.h"
#include "Document.h"

ImportWardDirectoryCommand::ImportWardDirectoryCommand(
    const QHash<QString, Family>& mergedFamilies,
    const QString& wardUnitNumber,
    const QString& wardName,
    const QString& description)
    : m_newFamilies(mergedFamilies)
    , m_wardUnitNumber(wardUnitNumber)
    , m_wardName(wardName)
    , m_description(description)
{
}

void ImportWardDirectoryCommand::execute(Document& document)
{
    // Save previous families for undo
    m_previousFamilies = document.families();

    // Set new families
    document.setFamilies(m_newFamilies);

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
