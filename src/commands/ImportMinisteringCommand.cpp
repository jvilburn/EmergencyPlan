#include "ImportMinisteringCommand.h"
#include "Document.h"

#include <QObject>

namespace
{

bool everyFamilyExists(const QSet<FamilyId>& ids, const Document& document)
{
    for (const FamilyId& id : ids)
    {
        if (!document.findFamilyById(id).has_value())
        {
            return false;
        }
    }
    return true;
}

bool personExists(const std::optional<PersonId>& id, const Document& document)
{
    return !id.has_value()
        || document.findPersonById(*id).has_value();
}

/// True when every person and family the districts and groups name is in document.
bool namesOnlyWhatExists(const MinisteringCollection& collection, const Document& document)
{
    for (const MinisteringGroup& group : collection.groups)
    {
        if (!everyPersonExists(group.ministerIds(), document)
            || !everyPersonExists(group.ministeredPersonIds(), document)
            || !everyFamilyExists(group.familyIds(), document)
            || !personExists(group.presidencyMemberId(), document))
        {
            return false;
        }
    }
    for (const MinisteringDistrict& district : collection.districts)
    {
        if (!personExists(district.presidencyMemberId(), document))
        {
            return false;
        }
    }
    return true;
}

QString orgToJson(MinisteringOrg org)
{
    switch (org)
    {
    case MinisteringOrg::EldersQuorum:
        return "eldersQuorum";
    case MinisteringOrg::ReliefSociety:
        return "reliefSociety";
    }
    return QString();
}

MinisteringOrg orgFromJson(const QJsonValue& value)
{
    if (value.toString() == "reliefSociety")
    {
        return MinisteringOrg::ReliefSociety;
    }
    return MinisteringOrg::EldersQuorum;
}

}  // namespace

// ============================================================================
// MinisteringCollection
// ============================================================================

MinisteringCollection MinisteringCollection::of(const Document& document, MinisteringOrg org)
{
    return MinisteringCollection{document.districts(org), document.groups(org)};
}

bool MinisteringCollection::operator==(const MinisteringCollection& other) const
{
    return districts == other.districts
        && groups == other.groups;
}

QJsonValue toJsonValue(const MinisteringCollection& value)
{
    QJsonObject districts;
    for (const MinisteringDistrict& district : value.districts)
    {
        districts[district.id().toString()] = district.toJson();
    }
    QJsonObject groups;
    for (const MinisteringGroup& group : value.groups)
    {
        groups[group.id().toString()] = group.toJson();
    }
    QJsonObject json;
    json["districts"] = districts;
    json["groups"] = groups;
    return json;
}

template<>
MinisteringCollection fromJsonValue<MinisteringCollection>(const QJsonValue& value)
{
    QJsonObject json = value.toObject();
    MinisteringCollection collection;
    QJsonObject districts = json["districts"].toObject();
    for (auto it = districts.constBegin(); it != districts.constEnd(); ++it)
    {
        MinisteringDistrict district = MinisteringDistrict::fromJson(it.value().toObject());
        collection.districts.insert(district.id(), district);
    }
    QJsonObject groups = json["groups"].toObject();
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it)
    {
        MinisteringGroup group = MinisteringGroup::fromJson(it.value().toObject());
        collection.groups.insert(group.id(), group);
    }
    return collection;
}

// ============================================================================
// ImportMinisteringCommand
// ============================================================================

UndoableCommandPtr ImportMinisteringCommand::fromImport(const Document& document,
                                                        const MinisteringImportResult& result,
                                                        const QString& fileName)
{
    MinisteringOrg org = result.isRSFormat ? MinisteringOrg::ReliefSociety : MinisteringOrg::EldersQuorum;
    FieldChange<MinisteringCollection> collection(MinisteringCollection::of(document, org),
                                                  MinisteringCollection{result.districts, result.groups});
    FamilyCollectionChange families = FamilyCollectionChange::between(document, result.families, {});

    std::optional<FieldChange<std::optional<QDate>>> pdfDate;
    if (result.pdfDate.has_value())
    {
        pdfDate = changeBetween(document.ministeringPdfDate(), result.pdfDate);
    }

    CommandDescription description(QObject::tr("Import the ministering assignments from %1.").arg(fileName), {});
    return std::make_shared<ImportMinisteringCommand>(org, collection, families, pdfDate,
                                                      document.wardDirectoryPdfDate(), description);
}

ImportMinisteringCommand::ImportMinisteringCommand(MinisteringOrg org,
                                                   const FieldChange<MinisteringCollection>& collection,
                                                   const FamilyCollectionChange& families,
                                                   const std::optional<FieldChange<std::optional<QDate>>>& pdfDate,
                                                   const std::optional<QDate>& wardDirectoryPdfDate,
                                                   const CommandDescription& description)
    : UndoableCommand(description)
    , m_org(org)
    , m_collection(collection)
    , m_families(families)
    , m_pdfDate(pdfDate)
    , m_wardDirectoryPdfDate(wardDirectoryPdfDate)
{
}

CommandOutcome ImportMinisteringCommand::execute(Document& document) const
{
    if (!m_collection.holds(MinisteringCollection::of(document, m_org))
        || !m_families.holds(document)
        || !changeHolds(m_pdfDate, document.ministeringPdfDate())
        || document.wardDirectoryPdfDate() != m_wardDirectoryPdfDate)
    {
        return CommandOutcome::Conflicted;
    }

    Document updated = document;
    m_families.applyTo(updated);
    if (!namesOnlyWhatExists(m_collection.after(), updated))
    {
        return CommandOutcome::Conflicted;
    }
    updated.setDistricts(m_org, m_collection.after().districts);
    updated.setGroups(m_org, m_collection.after().groups);
    updated.setMinisteringPdfDate(changedValue(m_pdfDate, document.ministeringPdfDate()));
    document = updated;
    return CommandOutcome::Applied;
}

UndoableCommandPtr ImportMinisteringCommand::inverse() const
{
    return std::make_shared<ImportMinisteringCommand>(m_org,
                                                      m_collection.swapped(),
                                                      m_families.swapped(),
                                                      changeSwapped(m_pdfDate),
                                                      m_wardDirectoryPdfDate,
                                                      description().undone());
}

QJsonObject ImportMinisteringCommand::toJson() const
{
    QJsonObject json;
    json["org"] = orgToJson(m_org);
    json["collection"] = changeToJson(m_collection);
    json["families"] = m_families.toJson();
    writeChange(json, "pdfDate", m_pdfDate);
    json["wardDirectoryPdfDate"] = toJsonValue(m_wardDirectoryPdfDate);
    return json;
}

DocumentChange ImportMinisteringCommand::documentChange() const
{
    return DocumentChange::full();
}

QList<Address> ImportMinisteringCommand::addressesGiven() const
{
    return m_families.addressesGiven();
}

CommandPtr ImportMinisteringCommand::fromJson(const QJsonObject& json, const CommandDescription& description)
{
    return std::make_shared<ImportMinisteringCommand>(orgFromJson(json["org"]),
                                                      changeFromJson<MinisteringCollection>(json["collection"].toObject()),
                                                      FamilyCollectionChange::fromJson(json["families"].toObject()),
                                                      readChange<std::optional<QDate>>(json, "pdfDate"),
                                                      fromJsonValue<std::optional<QDate>>(json["wardDirectoryPdfDate"]),
                                                      description);
}
