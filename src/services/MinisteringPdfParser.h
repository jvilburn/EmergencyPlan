#pragma once

#include <QString>
#include <QStringList>
#include <QHash>
#include <QDate>
#include <optional>

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
        bool isRSFormat = false;             // Final format: true for RS, false for EQ
        std::optional<bool> detectedFormat;  // Set when first family header detected

        QHash<MinisteringDistrictId, MinisteringDistrict> districts;
        QHash<MinisteringGroupId, MinisteringGroup> groups;

        // Families separated for deduplication during import
        QHash<FamilyId, Family> ministerFamilies;     // Families of ministers
        QHash<FamilyId, Family> ministeredFamilies;   // Families being ministered to

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
