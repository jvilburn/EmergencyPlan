#pragma once

#include "Family.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "MinisteringPdfParser.h"

#include <QDate>
#include <QString>
#include <QHash>
#include <optional>

/// Result of ministering import operation
struct MinisteringImportResult
{
    bool success = false;
    bool isRSFormat = false;  // Auto-detected: true for RS, false for EQ

    QHash<MinisteringDistrictId, MinisteringDistrict> districts;
    QHash<MinisteringGroupId, MinisteringGroup> groups;
    QHash<FamilyId, Family> families;  // Final merged family list

    QStringList errors;
    QString wardName;
    QString wardUnitNumber;
    QString stakeName;
    QString stakeUnitNumber;
    std::optional<QDate> pdfDate;  // From PDF footer (documentDate)
};

/// Imports ministering data from PDF files: resolves the PDF's people and families to the
/// existing ones and merges them with the existing families.
namespace MinisteringImportService
{
    /// Import ministering assignments from a PDF file (auto-detects EQ or RS): parses it,
    /// then merges it.
    MinisteringImportResult importFromPdf(
        const QString& pdfPath,
        const QHash<FamilyId, Family>& existingFamilies,
        std::optional<QDate> wardDirectoryDate);

    /// The import of parseResult onto existingFamilies.
    /// The PDF is authoritative for family data when wardDirectoryDate is not set, or when
    /// the PDF's date is set and is the same day as wardDirectoryDate or later. When it is
    /// not, family data from it is treated as stale: it only fills empty fields, and a family
    /// or person it cannot match is not added and is left out of the groups and districts.
    MinisteringImportResult merge(
        MinisteringPdfParser::ParseResult parseResult,
        const QHash<FamilyId, Family>& existingFamilies,
        std::optional<QDate> wardDirectoryDate);
}
