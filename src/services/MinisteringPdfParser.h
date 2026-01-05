#pragma once

#include <QString>
#include <QStringList>
#include <QHash>
#include <QDate>

#include "Person.h"
#include "Family.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"

namespace MinisteringPdfParser
{
    /// Result of parsing a ministering PDF
    struct ParseResult
    {
        bool success = false;
        bool isRSFormat = false;  // Auto-detected: true for RS, false for EQ

        QHash<QString, MinisteringDistrict> districts;
        QHash<QString, MinisteringGroup> groups;

        // Families separated for deduplication during import
        QHash<QString, Family> ministerFamilies;     // Families of ministers
        QHash<QString, Family> ministeredFamilies;   // Families being ministered to

        // Metadata from PDF header/footer
        QString wardName;
        QString wardUnitNumber;
        QString stakeName;
        QString stakeUnitNumber;
        QDate documentDate;  // From page footer (e.g., "21 Sep 2025")

        QStringList errors;  // Fatal parsing errors
    };

    /// Parse a ministering PDF file (auto-detects EQ or RS format).
    /// EQ format: Bold surname only in family column, ministers to families
    /// RS format: Bold "Last, First" name in family column, ministers to individuals
    /// @param pdfPath Path to the PDF file
    ParseResult parse(const QString& pdfPath);
}
