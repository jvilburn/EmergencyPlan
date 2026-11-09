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
    /// In order of display name, each parsed family takes the id of the existing family
    /// PersonMatching::findFamily finds among those no other parsed family has taken; a
    /// family that matches none keeps its new id. Within a matched family, each parsed
    /// member takes the id of the existing member PersonMatching::matchMembers matches it to,
    /// and a member that matches none keeps its new id. A matched family keeps its
    /// note, and a matched member keeps everything the directory does not print: only their
    /// name, phone, email, callings and whether they are a parent come from parsed.
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
