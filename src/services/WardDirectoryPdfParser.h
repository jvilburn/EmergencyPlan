#pragma once

#include <QString>
#include <QStringList>
#include <QHash>

#include "Coordinates.h"
#include "Family.h"
#include "PdfExtractor.h"
#include "Person.h"

namespace WardDirectoryPdfParser
{
    /// Result of parsing a ward directory PDF
    struct ParseResult
    {
        bool success = false;

        // All families from the directory
        QHash<FamilyId, Family> families;

        // The coordinates the directory prints for each family's address, keyed by
        // Address::full(); the directory import seeds the table of address coordinates with them
        QHash<QString, Coordinates> addressCoordinates;

        // Metadata from PDF header
        QString wardName;
        QString wardUnitNumber;

        QStringList errors;  // Fatal parsing errors
    };

    /// Parse a ward directory PDF file.
    /// @param pdfPath Path to the PDF file
    ParseResult parse(const QString& pdfPath);

    /// Parses a ward directory from each page's text fields, in reading order, as
    /// PdfExtractor::textFields gives them.
    ParseResult parseFields(const QList<QList<PdfTextField>>& pages);
}
