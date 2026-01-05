#pragma once

#include "Command.h"
#include "Family.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"

#include <QHash>

/// Command that handles RS ministering PDF import: sets districts, groups, and families.
/// This allows the entire import to be undone as a single operation.
class ImportRSMinisteringCommand : public Command
{
public:
    ImportRSMinisteringCommand(
        const QHash<QString, MinisteringDistrict>& districts,
        const QHash<QString, MinisteringGroup>& groups,
        const QHash<QString, Family>& families,
        const QString& description);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;

private:
    QHash<QString, MinisteringDistrict> m_newDistricts;
    QHash<QString, MinisteringGroup> m_newGroups;
    QHash<QString, Family> m_newFamilies;
    QString m_description;

    // For undo
    QHash<QString, MinisteringDistrict> m_previousDistricts;
    QHash<QString, MinisteringGroup> m_previousGroups;
    QHash<QString, Family> m_previousFamilies;
};
