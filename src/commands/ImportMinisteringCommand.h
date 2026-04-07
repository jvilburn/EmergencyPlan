#pragma once

#include "Command.h"
#include "Family.h"
#include "ItemType.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"

#include <QDate>
#include <QHash>
#include <optional>

/// Command that handles ministering PDF import: sets districts, groups, and families.
/// This allows the entire import to be undone as a single operation.
class ImportMinisteringCommand : public Command
{
public:
    ImportMinisteringCommand(
        MinisteringOrg org,
        const QHash<MinisteringDistrictId, MinisteringDistrict>& districts,
        const QHash<MinisteringGroupId, MinisteringGroup>& groups,
        const QHash<FamilyId, Family>& families,
        std::optional<QDate> pdfDate,
        const QString& description);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    MinisteringOrg m_org;
    QHash<MinisteringDistrictId, MinisteringDistrict> m_newDistricts;
    QHash<MinisteringGroupId, MinisteringGroup> m_newGroups;
    QHash<FamilyId, Family> m_newFamilies;
    QString m_description;
    std::optional<QDate> m_pdfDate;

    // For undo
    QHash<MinisteringDistrictId, MinisteringDistrict> m_previousDistricts;
    QHash<MinisteringGroupId, MinisteringGroup> m_previousGroups;
    QHash<FamilyId, Family> m_previousFamilies;
    std::optional<QDate> m_previousMinisteringPdfDate;
};
