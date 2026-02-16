#pragma once

#include "Family.h"
#include "WardDirectoryPdfParser.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QSet>
#include <QDate>
#include <optional>

/// Result of ward directory import operation
struct WardDirectoryImportResult
{
    bool success = false;
    QHash<FamilyId, Family> families;
    QSet<FamilyId> removedFamilyIds;  // Families in existing but not in PDF
    QStringList errors;
    QString wardName;
    QString wardUnitNumber;
    std::optional<QDate> pdfDate;  // File modification date as proxy
};

/// Service for importing ward directory data from PDF files
class WardDirectoryImportService : public QObject
{
    Q_OBJECT

public:
    explicit WardDirectoryImportService(QObject* parent = nullptr);
    ~WardDirectoryImportService();

    /// Import families from a PDF file.
    /// Preserves IDs for families/persons that match existing data.
    WardDirectoryImportResult importFromPdf(
        const QString& pdfPath,
        const QHash<FamilyId, Family>& existingFamilies,
        std::optional<QDate> ministeringPdfDate = std::nullopt);

private:
    std::optional<Family> findMatchingFamily(
        const Family& pdfFamily,
        const QHash<FamilyId, Family>& families);

    std::optional<Person> findMatchingPerson(
        const Person& person,
        const Family& family);

    Family preserveIds(
        const Family& parsedFamily,
        const Family& existingFamily);
};
