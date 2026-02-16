#pragma once

#include <QString>
#include <QStringList>
#include <QHash>

#include "Family.h"
#include "Person.h"

namespace WardDirectoryPdfParser
{
    /// Result of parsing a ward directory PDF
    struct ParseResult
    {
        bool success = false;

        // All families from the directory
        QHash<FamilyId, Family> families;

        // Metadata from PDF header
        QString wardName;
        QString wardUnitNumber;

        QStringList errors;  // Fatal parsing errors
    };

    /// Parse a ward directory PDF file.
    /// @param pdfPath Path to the PDF file
    ParseResult parse(const QString& pdfPath);
}
