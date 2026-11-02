#pragma once

#include "Command.h"
#include "FamilyChange.h"
#include "FieldChange.h"
#include "FieldChangeJson.h"
#include "ItemType.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "MinisteringImportService.h"

#include <QDate>
#include <QHash>
#include <optional>

/// An organization's districts and groups, held as one value. The parser gives every group
/// and district a new id on each import, so an import changes the whole collection.
struct MinisteringCollection
{
    QHash<MinisteringDistrictId, MinisteringDistrict> districts;
    QHash<MinisteringGroupId, MinisteringGroup> groups;

    /// The organization's districts and groups as document holds them.
    static MinisteringCollection of(const Document& document, MinisteringOrg org);

    bool operator==(const MinisteringCollection& other) const;
    bool operator!=(const MinisteringCollection& other) const { return !(*this == other); }
};

QJsonValue toJsonValue(const MinisteringCollection& value);
template<>
MinisteringCollection fromJsonValue<MinisteringCollection>(const QJsonValue& value);

/// A ministering import, worked out at construction: its organization's districts and
/// groups, the families it adds or changes, and the ministering PDF date. The districts and
/// groups name only people and families that exist once it applies.
class ImportMinisteringCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "importMinistering";

    /// The import of result, read from the file named fileName, onto document.
    static UndoableCommandPtr fromImport(const Document& document,
                                         const MinisteringImportResult& result,
                                         const QString& fileName);

    ImportMinisteringCommand(MinisteringOrg org,
                             const FieldChange<MinisteringCollection>& collection,
                             const FamilyCollectionChange& families,
                             const std::optional<FieldChange<std::optional<QDate>>>& pdfDate,
                             const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;
    QList<Address> addressesGiven() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    MinisteringOrg m_org;
    FieldChange<MinisteringCollection> m_collection;
    FamilyCollectionChange m_families;
    std::optional<FieldChange<std::optional<QDate>>> m_pdfDate;
};
