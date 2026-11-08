#include "ImportWardDirectoryCommand.h"
#include "Document.h"
#include "FieldChangeJson.h"

#include <QObject>

UndoableCommandPtr ImportWardDirectoryCommand::fromImport(const Document& document,
                                                          const WardDirectoryImportResult& result,
                                                          const QString& fileName)
{
    FamilyCollectionChange families = FamilyCollectionChange::between(document, result.families, result.removedFamilyIds);
    std::optional<FieldChange<std::optional<QDate>>> pdfDate = changeBetween(document.wardDirectoryPdfDate(), result.pdfDate);

    QHash<QString, Coordinates> coordinates;
    for (auto it = result.addressCoordinates.constBegin(); it != result.addressCoordinates.constEnd(); ++it)
    {
        if (!document.addressCoordinates().contains(it.key()))
        {
            coordinates.insert(it.key(), it.value());
        }
    }

    Ward ward;
    ward.setUnitNumber(result.wardUnitNumber);
    ward.setName(result.wardName);

    bool wardAlreadyThere = document.metadata().findWardByUnit(ward.unitNumber()).has_value();
    if (families.isEmpty() && !pdfDate.has_value() && coordinates.isEmpty() && wardAlreadyThere)
    {
        return nullptr;
    }

    CommandDescription description(QObject::tr("Import the ward directory from %1.").arg(fileName), {});
    return std::make_shared<ImportWardDirectoryCommand>(families, pdfDate, coordinates, ward,
                                                        document.ministeringPdfDate(), description);
}

ImportWardDirectoryCommand::ImportWardDirectoryCommand(const FamilyCollectionChange& families,
                                                       const std::optional<FieldChange<std::optional<QDate>>>& pdfDate,
                                                       const QHash<QString, Coordinates>& coordinates,
                                                       const Ward& ward,
                                                       const std::optional<QDate>& ministeringPdfDate,
                                                       const CommandDescription& description)
    : UndoableCommand(description)
    , m_families(families)
    , m_pdfDate(pdfDate)
    , m_coordinates(coordinates)
    , m_ward(ward)
    , m_ministeringPdfDate(ministeringPdfDate)
{
}

CommandOutcome ImportWardDirectoryCommand::execute(Document& document) const
{
    if (!m_families.holds(document)
        || !changeHolds(m_pdfDate, document.wardDirectoryPdfDate())
        || document.ministeringPdfDate() != m_ministeringPdfDate)
    {
        return CommandOutcome::Conflicted;
    }

    m_families.applyTo(document);
    document.setWardDirectoryPdfDate(changedValue(m_pdfDate, document.wardDirectoryPdfDate()));
    for (auto it = m_coordinates.constBegin(); it != m_coordinates.constEnd(); ++it)
    {
        if (!document.addressCoordinates().contains(it.key()))
        {
            document.setAddressCoordinates(it.key(), it.value());
        }
    }
    if (!document.metadata().findWardByUnit(m_ward.unitNumber()).has_value())
    {
        document.metadata().updateWard(m_ward);
    }
    return CommandOutcome::Applied;
}

UndoableCommandPtr ImportWardDirectoryCommand::inverse() const
{
    return std::make_shared<ImportWardDirectoryCommand>(m_families.swapped(),
                                                        changeSwapped(m_pdfDate),
                                                        QHash<QString, Coordinates>(),
                                                        m_ward,
                                                        m_ministeringPdfDate,
                                                        description().undone());
}

QJsonObject ImportWardDirectoryCommand::toJson() const
{
    QJsonObject json;
    json["families"] = m_families.toJson();
    writeChange(json, "pdfDate", m_pdfDate);
    writeAddressCoordinates(json, "coordinates", m_coordinates);
    json["ward"] = m_ward.toJson();
    json["ministeringPdfDate"] = toJsonValue(m_ministeringPdfDate);
    return json;
}

DocumentChange ImportWardDirectoryCommand::documentChange() const
{
    return DocumentChange::full();
}

QList<Address> ImportWardDirectoryCommand::addressesGiven() const
{
    return m_families.addressesGiven();
}

QStringList ImportWardDirectoryCommand::unitsGiven() const
{
    return {m_ward.unitNumber()};
}

CommandPtr ImportWardDirectoryCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    FamilyCollectionChange families = FamilyCollectionChange::fromJson(json.object("families"));
    std::optional<FieldChange<std::optional<QDate>>> pdfDate = readChange<std::optional<QDate>>(json, "pdfDate");
    QHash<QString, Coordinates> coordinates = readAddressCoordinates(json, "coordinates");
    Ward ward = Ward::fromJson(json.object("ward"));
    std::optional<QDate> ministeringPdfDate = readValue<std::optional<QDate>>(json, "ministeringPdfDate");
    json.finish();
    return std::make_shared<ImportWardDirectoryCommand>(families, pdfDate, coordinates, ward, ministeringPdfDate,
                                                        description);
}
