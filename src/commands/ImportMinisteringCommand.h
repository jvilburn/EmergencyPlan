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
MinisteringCollection readValue<MinisteringCollection>(JsonReader& json, const QString& key);

/// A ministering import, worked out at construction: its organization's districts and
/// groups, the families it adds or changes, the ministering PDF date, and the ward directory
/// PDF date the import read when deciding whether it is authoritative. The districts and
/// groups name only people and families that exist once it applies. It conflicts with an
/// edit to something it changes, and when the ward directory PDF date is no longer the one
/// it read.
class ImportMinisteringCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "importMinistering";

    /// The import of result, read from the file named fileName, onto document. result is the
    /// merge onto document's families and its ward directory PDF date.
    static UndoableCommandPtr fromImport(const Document& document,
                                         const MinisteringImportResult& result,
                                         const QString& fileName);

    ImportMinisteringCommand(MinisteringOrg org,
                             const FieldChange<MinisteringCollection>& collection,
                             const FamilyCollectionChange& families,
                             const std::optional<FieldChange<std::optional<QDate>>>& pdfDate,
                             const std::optional<QDate>& wardDirectoryPdfDate,
                             const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    CommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;
    QList<Address> addressesGiven() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    MinisteringOrg m_org;
    FieldChange<MinisteringCollection> m_collection;
    FamilyCollectionChange m_families;
    std::optional<FieldChange<std::optional<QDate>>> m_pdfDate;
    std::optional<QDate> m_wardDirectoryPdfDate;
};
