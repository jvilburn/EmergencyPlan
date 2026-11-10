#pragma once

#include "Command.h"
#include "Coordinates.h"
#include "FamilyChange.h"
#include "FieldChange.h"
#include "Ward.h"
#include "WardDirectoryImportService.h"

#include <QDate>
#include <QHash>
#include <optional>

/// A ward directory import, worked out at construction: the families it adds, changes and
/// removes, with the cleanup of the families and members it removes; the directory PDF
/// date; the coordinates the PDF gives for addresses the table lacks; the ward record; and
/// the ministering PDF date the import read when deciding which families to remove. It
/// conflicts with an edit to something it changes, and when the ministering PDF date is no
/// longer the one it read.
class ImportWardDirectoryCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "importWardDirectory";

    /// The import of result, read from the file named fileName, onto document, or an empty
    /// pointer when it changes nothing, which WardSession::commit takes as nothing to do.
    /// result is the merge onto document's families and its ministering PDF date.
    static UndoableCommandPtr fromImport(const Document& document,
                                         const WardDirectoryImportResult& result,
                                         const QString& fileName);

    /// coordinates holds, keyed by Address::full(), the entries to add where the table lacks
    /// one; ward is the ward record to create when the document lacks one with its unit
    /// number.
    ImportWardDirectoryCommand(const FamilyCollectionChange& families,
                               const std::optional<FieldChange<std::optional<QDate>>>& pdfDate,
                               const QHash<QString, Coordinates>& coordinates,
                               const Ward& ward,
                               const std::optional<QDate>& ministeringPdfDate,
                               const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;

    /// The family and date changes swapped. The coordinates and the ward record stay in
    /// place.
    CommandPtr inverse() const override;

    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;
    QList<Address> addressesGiven() const override;

    /// The unit of the ward record it creates or keeps.
    QStringList unitsGiven() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    FamilyCollectionChange m_families;
    std::optional<FieldChange<std::optional<QDate>>> m_pdfDate;
    QHash<QString, Coordinates> m_coordinates;
    Ward m_ward;
    std::optional<QDate> m_ministeringPdfDate;
};
