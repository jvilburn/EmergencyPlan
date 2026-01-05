#include "WardDirectoryPdfParser.h"
#include "PdfExtractor.h"
#include "PdfFieldUtils.h"
#include "Address.h"
#include "Calling.h"
#include "Email.h"
#include "Name.h"
#include "Phone.h"

#include <QRegularExpression>
#include <cmath>
#include <optional>

namespace
{
    // ============================================================================
    // Layout Constants
    // ============================================================================

    // Tolerances for position-based grouping
    constexpr double X_TOLERANCE = 15.0;
    constexpr double Y_GROUP_TOLERANCE = 2.0;

    // Font size thresholds (from PDF analysis)
    constexpr float FONT_SIZE_HEADER = 13.0f;
    constexpr float FONT_SIZE_SECTION = 12.0f;
    constexpr float FONT_SIZE_FAMILY = 11.0f;
    constexpr float FONT_SIZE_NAME = 9.0f;
    constexpr float FONT_SIZE_ADDRESS = 8.0f;
    constexpr float FONT_SIZE_DETAIL = 7.0f;
    constexpr float FONT_SIZE_FOOTER = 5.0f;
    constexpr float FONT_SIZE_TOLERANCE = 1.0f;

    // X position thresholds (from PDF analysis)
    constexpr double X_FAMILY = 32.0;
    constexpr double X_PRIMARY = 50.0;
    constexpr double X_CHILD_COL2 = 229.0;
    constexpr double X_ADDRESS = 310.0;
    constexpr double X_SECONDARY = 319.0;
    constexpr double X_CHILD_COL3 = 408.0;

    // ============================================================================
    // Regex Patterns
    // ============================================================================

    // Matches coordinates like "35.873964, -80.589959"
    const QRegularExpression COORDINATES_PATTERN(
        R"(^(-?\d+\.\d+),\s*(-?\d+\.\d+)$)");

    // ============================================================================
    // Helper Functions
    // ============================================================================

    bool fontSizeMatches(float actual, float expected)
    {
        return std::abs(actual - expected) < FONT_SIZE_TOLERANCE;
    }

    bool isNearX(double fieldX, double targetX)
    {
        return std::abs(fieldX - targetX) <= X_TOLERANCE;
    }

    bool isPageHeader(const PdfTextField& field)
    {
        return fontSizeMatches(field.fontSize(), FONT_SIZE_HEADER);
    }

    bool isPageFooter(const PdfTextField& field)
    {
        return fontSizeMatches(field.fontSize(), FONT_SIZE_FOOTER);
    }

    bool isSectionLetter(const PdfTextField& field)
    {
        // Section letters are single uppercase letters at font 12
        if (!fontSizeMatches(field.fontSize(), FONT_SIZE_SECTION))
        {
            return false;
        }
        QString text = field.text().trimmed();
        return text.length() == 1 && text[0].isUpper();
    }

    bool isFamilyHeader(const PdfTextField& field)
    {
        return field.bold()
            && fontSizeMatches(field.fontSize(), FONT_SIZE_FAMILY)
            && isNearX(field.left(), X_FAMILY);
    }

    bool isAddressLine(const PdfTextField& field)
    {
        return fontSizeMatches(field.fontSize(), FONT_SIZE_ADDRESS)
            && field.left() >= X_ADDRESS - X_TOLERANCE;
    }

    bool isFamilyStart(const PdfTextField& field)
    {
        return isFamilyHeader(field) || isAddressLine(field);
    }

    bool isCoordinates(const PdfTextField& field)
    {
        if (!fontSizeMatches(field.fontSize(), FONT_SIZE_DETAIL))
        {
            return false;
        }
        if (field.left() < X_ADDRESS - X_TOLERANCE)
        {
            return false;
        }
        return COORDINATES_PATTERN.match(field.text().trimmed()).hasMatch();
    }

    bool isPersonName(const PdfTextField& field)
    {
        return fontSizeMatches(field.fontSize(), FONT_SIZE_NAME);
    }

    bool isPrimaryPersonName(const PdfTextField& field)
    {
        return isPersonName(field) && isNearX(field.left(), X_PRIMARY);
    }

    bool isPersonDetail(const PdfTextField& field)
    {
        return fontSizeMatches(field.fontSize(), FONT_SIZE_DETAIL)
            && field.left() < X_ADDRESS - X_TOLERANCE;
    }

    // ============================================================================
    // Header Parsing
    // ============================================================================

    void parseDirectoryHeader(const QString& headerText, QString& wardName, QString& unitNumber)
    {
        // Input: "Example Ward - 123456"
        int dashPos = headerText.lastIndexOf(" - ");
        if (dashPos < 0)
        {
            wardName = headerText.trimmed();
            return;
        }
        wardName = headerText.left(dashPos).trimmed();
        unitNumber = headerText.mid(dashPos + 3).trimmed();
    }

    // ============================================================================
    // Coordinate Parsing
    // ============================================================================

    std::pair<std::optional<double>, std::optional<double>> parseCoordinates(const QString& text)
    {
        QRegularExpressionMatch match = COORDINATES_PATTERN.match(text.trimmed());
        if (!match.hasMatch())
        {
            return {std::nullopt, std::nullopt};
        }
        double lat = match.captured(1).toDouble();
        double lng = match.captured(2).toDouble();
        return {lat, lng};
    }

    // ============================================================================
    // Person Parsing
    // ============================================================================

    /// Parsed person data before creating Person object
    struct ParsedPerson
    {
        QString givenNames;
        Phone phone;
        Email email;
        QStringList callings;
    };

    /// Parse detail fields for a person (phone, email, callings)
    void parsePersonDetails(const QList<PdfTextField>& detailFields, ParsedPerson& person)
    {
        for (const PdfTextField& field : detailFields)
        {
            QString text = field.text().trimmed();
            if (text.isEmpty())
            {
                continue;
            }

            if (person.phone.isEmpty() && Phone::isPhone(text))
            {
                person.phone = Phone(text);
            }
            else if (person.email.isEmpty() && Email::isEmail(text))
            {
                person.email = Email(text);
            }
            else if (Calling::isCalling(text))
            {
                person.callings.append(text);
            }
        }
    }

    /// Determine which parent column a field is in based on X position
    /// Returns 0=primary, 1=secondary, or nullopt if neither
    std::optional<int> getParentColumnIndex(double x)
    {
        if (isNearX(x, X_PRIMARY))
        {
            return 0;
        }
        if (isNearX(x, X_SECONDARY))
        {
            return 1;
        }
        return std::nullopt;
    }

    /// Determine family header column index based on X position
    /// Returns 0=family name column, 1=address column, or nullopt if neither
    std::optional<int> getFamilyHeaderColumnIndex(double x)
    {
        if (isNearX(x, X_FAMILY))
        {
            return 0;
        }
        if (isNearX(x, X_ADDRESS))
        {
            return 1;
        }
        return std::nullopt;
    }

    /// Determine child column index based on X position
    /// Returns 0=child col 1, 1=child col 2, 2=child col 3, or nullopt if none
    std::optional<int> getChildColumnIndex(double x)
    {
        if (isNearX(x, X_PRIMARY))
        {
            return 0;
        }
        if (isNearX(x, X_CHILD_COL2))
        {
            return 1;
        }
        if (isNearX(x, X_CHILD_COL3))
        {
            return 2;
        }
        return std::nullopt;
    }

    /// Parse a person from a column's fields.
    /// Returns nullopt if no valid person name is found.
    std::optional<Person> parsePersonFromFields(
        const QList<PdfTextField>& fields,
        const QString& surname,
        bool isParent)
    {
        if (fields.isEmpty())
        {
            return std::nullopt;
        }

        // First field with name font size is the person's name
        // Remaining fields with detail font size are phone/email/callings
        QString givenNames;
        QList<PdfTextField> detailFields;

        for (const PdfTextField& field : fields)
        {
            if (fontSizeMatches(field.fontSize(), FONT_SIZE_NAME) && givenNames.isEmpty())
            {
                givenNames = field.text().trimmed();
            }
            else if (fontSizeMatches(field.fontSize(), FONT_SIZE_DETAIL))
            {
                detailFields.append(field);
            }
        }

        if (givenNames.isEmpty())
        {
            return std::nullopt;
        }

        ParsedPerson parsed;
        parsed.givenNames = givenNames;
        parsePersonDetails(detailFields, parsed);

        return Person::create(
            Name(surname, parsed.givenNames),
            isParent,
            parsed.phone,
            Phone(),  // altPhone
            parsed.email,
            std::nullopt,  // gender (not in directory)
            Birthday(),
            parsed.callings
        );
    }

    // ============================================================================
    // Family Parsing
    // ============================================================================

    /// Parse a single family from its fields
    Family parseFamily(const QList<PdfTextField>& contentFields)
    {
        auto fieldIter = contentFields.constBegin();

        // Gather Family Header columns
        QList<PdfTextField> familyHeaderColumns[2];  // 0=primary, 1=secondary        
        while (fieldIter != contentFields.constEnd())
        {
            const PdfTextField& field = *fieldIter;
            std::optional<int> familyHeaderColumnIndex = getFamilyHeaderColumnIndex(field.left());
            if (familyHeaderColumnIndex.has_value())
            {
                familyHeaderColumns[familyHeaderColumnIndex.value()].append(field);
                ++fieldIter;
                
            }
            else
            {
                break;
            }
        }
        // Gather parent fields (one person per column)
        QList<PdfTextField> primaryParentFields;
        QList<PdfTextField> secondaryParentFields;
        bool primaryNameFound = false;

        while (fieldIter != contentFields.constEnd())
        {
            const PdfTextField& field = *fieldIter;

            if (isNearX(field.left(), X_PRIMARY))
            {
                // Second name at primary position means we've reached children
                if (fontSizeMatches(field.fontSize(), FONT_SIZE_NAME) && primaryNameFound)
                {
                    break;
                }
                if (fontSizeMatches(field.fontSize(), FONT_SIZE_NAME))
                {
                    primaryNameFound = true;
                }
                primaryParentFields.append(field);
                ++fieldIter;
            }
            else if (isNearX(field.left(), X_SECONDARY))
            {
                secondaryParentFields.append(field);
                ++fieldIter;
            }
            else
            {
                break;
            }
        }
        // Gather Child columns
        QList<PdfTextField> childColumns[3];  // 0=col1, 1=col2, 2=col3
        while (fieldIter != contentFields.constEnd())
        {
            const PdfTextField& field = *fieldIter;
            std::optional<int> childColumnIndex = getChildColumnIndex(field.left());
            if (childColumnIndex.has_value())
            {
                childColumns[childColumnIndex.value()].append(field);
                ++fieldIter;
            }
            else
            {
                break;
            }
        }

        // ====================================================================
        // Parse family header (column 0 = family name, column 1 = address)
        // ====================================================================
        QString familyName;
        Address address;
        std::optional<double> latitude;
        std::optional<double> longitude;

        // Family name from column 0
        if (!familyHeaderColumns[0].isEmpty())
        {
            familyName = familyHeaderColumns[0].first().text().trimmed();
        }

        // Address and coordinates from column 1
        for (const PdfTextField& field : familyHeaderColumns[1])
        {
            QString text = field.text().trimmed();
            if (text.isEmpty())
            {
                continue;
            }

            // Check if this is coordinates
            auto [lat, lng] = parseCoordinates(text);
            if (lat.has_value())
            {
                latitude = lat;
                longitude = lng;
            }
            else
            {
                // It's an address line
                address.addLine(text);
            }
        }

        // Extract surname from family name using Name
        Name familyNameParsed(familyName);
        QString surname = familyNameParsed.surname();

        // ====================================================================
        // Parse persons from columns
        // ====================================================================
        QList<Person> members;

        // Primary parent
        if (auto person = parsePersonFromFields(primaryParentFields, surname, true))
        {
            members.append(*person);
        }

        // Secondary parent
        if (auto person = parsePersonFromFields(secondaryParentFields, surname, true))
        {
            members.append(*person);
        }

        // Children from all three columns
        for (int col = 0; col < 3; ++col)
        {
            if (auto person = parsePersonFromFields(childColumns[col], surname, false))
            {
                members.append(*person);
            }
        }

        // ====================================================================
        // Build and return family
        // ====================================================================
        return Family::create(latitude, longitude, address, members);
    }

}  // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

WardDirectoryPdfParser::ParseResult WardDirectoryPdfParser::parse(const QString& pdfPath)
{
    ParseResult result;
    result.success = false;

    PdfExtractor extractor;
    if (!extractor.open(pdfPath))
    {
        result.errors.append(QString("Failed to open PDF: %1").arg(pdfPath));
        return result;
    }

    int numPages = extractor.pageCount();
    if (numPages == 0)
    {
        result.errors.append("PDF has no pages");
        return result;
    }

    // ========================================================================
    // Phase 1: Load all pages, extract metadata, collect content fields
    // ========================================================================

    // Collect all family blocks: (familyName, fields)
    QList<QList<PdfTextField>> familyBlocks;
    QList<PdfTextField> currentFields;

    for (int pageNum = 0; pageNum < numPages; ++pageNum)
    {
        QList<PdfTextField> pageFields = extractor.textFields(pageNum);
        if (pageFields.isEmpty())
        {
            continue;
        }

        for (int i = 0; i < pageFields.size(); ++i)
        {
            const PdfTextField& field = pageFields[i];

            // Skip page-level elements
            if (isPageHeader(field))
            {
                // Extract ward info from first page header
                if (pageNum == 0 && result.wardName.isEmpty())
                {
                    parseDirectoryHeader(field.text(), result.wardName, result.wardUnitNumber);
                }
                continue;
            }

            if (isPageFooter(field) || isSectionLetter(field))
            {
                continue;
            }

            // Family header starts a new block
            if (isFamilyStart(field))
            {
                if (!currentFields.isEmpty())
                {
                    familyBlocks.append({currentFields});
                }
                currentFields.clear();
                currentFields.append(field);

                // Gather all header fields until we hit the first person name
                while (i + 1 < pageFields.size() && !isPrimaryPersonName(pageFields[i + 1]))
                {
                    ++i;
                    currentFields.append(pageFields[i]);
                }
                continue;
            }

            // Accumulate content fields for current family
            if (!currentFields.isEmpty())
            {
                currentFields.append(field);
            }
        }
    }

    // Don't forget the last family
    if (!currentFields.isEmpty())
    {
        familyBlocks.append({currentFields});
    }

    // ========================================================================
    // Phase 2: Parse each family block into model objects
    // ========================================================================

    for (QList<PdfTextField>& block : familyBlocks)
    {
        Family family = parseFamily(block);
        result.families.insert(family.id(), family);
    }

    result.success = true;
    return result;
}
