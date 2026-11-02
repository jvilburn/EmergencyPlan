#pragma once

#include "Family.h"
#include "WardDirectoryPdfParser.h"

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
    QHash<QString, Coordinates> addressCoordinates;  // The coordinates the PDF prints, keyed by Address::full()
    QStringList errors;
    QString wardName;
    QString wardUnitNumber;
    std::optional<QDate> pdfDate;  // File modification date as proxy
};

/// Imports ward directory data from PDF files.
namespace WardDirectoryImportService
{
    /// Import families from a PDF file: parses it, then merges it, dated by the file's
    /// modification date.
    WardDirectoryImportResult importFromPdf(
        const QString& pdfPath,
        const QHash<FamilyId, Family>& existingFamilies,
        std::optional<QDate> ministeringPdfDate);

    /// The import of parsed, from a PDF dated pdfDate, onto existingFamilies.
    ///
    /// Each parsed family takes the id of the first existing family with the same surname
    /// and display name, ignoring case, that no other parsed family has taken; a family that
    /// matches none keeps its new id. Within a matched family, each existing member is
    /// matched at most once, in two passes: each parsed member takes the id of an existing
    /// member with the same given names, ignoring case; then each parsed member still
    /// unmatched takes the id of the first remaining existing member with the same first
    /// given name. A member that matches none keeps its new id.
    ///
    /// An existing family no parsed family matches is removed when ministeringPdfDate is not
    /// set, or when pdfDate is the same day as ministeringPdfDate or later; otherwise it is
    /// kept as it is.
    WardDirectoryImportResult merge(
        const WardDirectoryPdfParser::ParseResult& parsed,
        const QHash<FamilyId, Family>& existingFamilies,
        std::optional<QDate> pdfDate,
        std::optional<QDate> ministeringPdfDate);
}
