#include "MinisteringPdfParser.h"
#include "PdfExtractor.h"
#include "PdfFieldUtils.h"
#include "Birthday.h"
#include "Email.h"
#include "Gender.h"
#include "Name.h"
#include "Phone.h"
#include "Address.h"

#include <QSet>
#include <QRegularExpression>
#include <QUuid>
#include <cmath>
#include <limits>
#include <optional>

namespace
{
    // ============================================================================
    // Layout Constants
    // ============================================================================

    // Tolerances for position-based grouping
    constexpr double X_TOLERANCE = 15.0;  // How close X must be to detected column
    constexpr double Y_GROUP_TOLERANCE = 2.0;  // Max Y difference for same-line grouping
    constexpr double PARENT_CHILD_THRESHOLD = 8.0;  // X indent cutoff for parent vs child (children ~13pts indented)

    // Content detection thresholds
    constexpr int DOT_SEPARATOR_MIN_COUNT = 10;  // Minimum dots to identify separator line
    constexpr float FONT_SIZE_TOLERANCE = 1.0f;  // Max difference for font size matching

    // ============================================================================
    // Regex Patterns (module-level for thread safety)
    // ============================================================================

    // Matches footer date like "21 Sep 2025"
    const QRegularExpression DATE_PATTERN(
        R"(^(\d{1,2})\s+(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\s+(\d{4})$)");

    // Matches birthday like "7 Oct" or "23 May (16)"
    const QRegularExpression BIRTHDAY_PATTERN(
        R"(^(\d{1,2})\s+(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)(?:\s+\((\d+)\))?$)");

    // Month name to number mapping
    const QHash<QString, int> MONTH_MAP = {
        {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4},
        {"May", 5}, {"Jun", 6}, {"Jul", 7}, {"Aug", 8},
        {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
    };

    // Detected column X positions for a companionship
    struct ColumnPositions
    {
        std::optional<double> ministerX;    // Bold "Last, First" at leftmost position
        std::optional<double> familyX;      // Bold surname or full name (to right of minister)
        std::optional<double> parentNameX;  // Leftmost full name in member column (parents are flush left)
        std::optional<double> genderX;      // "Male" or "Female"
        std::optional<double> birthdayX;    // Date pattern like "7 Oct"
        bool isRSFormat = false;            // True if family header is full name (RS), false if surname only (EQ)
    };

    // ============================================================================
    // Birthday Parsing Helper (local to this parser)
    // ============================================================================

    /// Parse birthday text like "7 Oct" or "23 May (16)" into Birthday object.
    /// Uses documentDate to calculate birth year from age if present.
    Birthday parseBirthday(const QString& text, const QDate& documentDate)
    {
        QRegularExpressionMatch match = BIRTHDAY_PATTERN.match(text.trimmed());
        if (!match.hasMatch())
        {
            return Birthday();
        }

        int day = match.captured(1).toInt();
        int month = MONTH_MAP.value(match.captured(2), 0);
        if (month == 0)
        {
            return Birthday();
        }

        std::optional<int> year;
        if (match.capturedLength(3) > 0)
        {
            int age = match.captured(3).toInt();
            // Calculate birth year from age relative to document date
            int birthYear = documentDate.year() - age;
            // Adjust if birthday hasn't occurred yet this year
            if (QDate(birthYear, month, day) > documentDate)
            {
                birthYear--;
            }
            year = birthYear;
        }

        return Birthday::create(year, month, day);
    }

    // ============================================================================
    // Content Heuristics
    // ============================================================================

    bool isDotSeparatorLine(const QList<PdfTextField>& linesOnSameY)
    {
        // Count dots on this line (Dart uses 10+ dots)
        int dotCount = 0;
        for (const PdfTextField& field : linesOnSameY)
        {
            QString text = field.text().trimmed();
            for (const QChar& ch : text)
            {
                if (ch == '.')
                {
                    ++dotCount;
                }
                else if (!ch.isSpace())
                {
                    return false;  // Non-dot, non-space = not a separator
                }
            }
        }
        return dotCount >= DOT_SEPARATOR_MIN_COUNT;
    }

    bool isPresidencyMemberLabel(const QString& text)
    {
        return text.trimmed() == "Presidency Member:";
    }

    bool isDistrictTitle(float fontSize, float districtFontSize)
    {
        return std::abs(fontSize - districtFontSize) < FONT_SIZE_TOLERANCE;
    }

    /// Concatenate text from fields matching a predicate (handles wrapped text).
    template<typename Predicate>
    QString concatenateMatchingFields(const QList<PdfTextField>& fields, Predicate matches)
    {
        QString result;
        for (const PdfTextField& field : fields)
        {
            if (matches(field))
            {
                if (!result.isEmpty())
                {
                    result += " ";
                }
                result += field.text().trimmed();
            }
        }
        return result;
    }

    // Line height tolerance for detecting wrapped text (consecutive Y positions)
    constexpr double Y_WRAP_TOLERANCE = 12.0;

    /// Merge consecutive bold fields at similar X positions (handles wrapped family headers).
    /// Fields must already be sorted by Y (top to bottom). Modifies list in place.
    /// Looks for bold@targetX fields that are consecutive by Y, ignoring intervening fields.
    void mergeWrappedBoldFields(QList<PdfTextField>& fields, double targetX)
    {
        // Find indices of all bold fields at targetX
        QList<int> targetIndices;
        for (int i = 0; i < fields.size(); ++i)
        {
            if (fields[i].bold() && std::abs(fields[i].left() - targetX) <= X_TOLERANCE)
            {
                targetIndices.append(i);
            }
        }

        // Process in reverse order so removals don't invalidate earlier indices
        for (int t = targetIndices.size() - 1; t > 0; --t)
        {
            int currIdx = targetIndices[t];
            int prevIdx = targetIndices[t - 1];
            const PdfTextField& curr = fields[currIdx];
            const PdfTextField& prev = fields[prevIdx];

            bool consecutiveY = (curr.y() - prev.y()) <= Y_WRAP_TOLERANCE;
            if (consecutiveY)
            {
                // Merge curr into prev
                QString mergedText = prev.text().trimmed() + " " + curr.text().trimmed();
                fields[prevIdx] = PdfTextField(
                    mergedText,
                    prev.type(),
                    prev.left(),
                    std::max(prev.right(), curr.right()),
                    prev.y(),
                    prev.fontSize(),
                    prev.bold()
                );
                fields.removeAt(currIdx);
            }
        }
    }

    // ============================================================================
    // Font Size Detection
    // ============================================================================

    float detectDistrictFontSize(const QList<PdfTextField>& fields)
    {
        // Find the largest font size (district titles)
        float maxSize = 0;
        for (const PdfTextField& field : fields)
        {
            if (field.fontSize() > maxSize)
            {
                maxSize = field.fontSize();
            }
        }
        return maxSize;
    }

    // ============================================================================
    // Column Position Detection
    // ============================================================================

    ColumnPositions detectColumnPositions(const QList<PdfTextField>& fields)
    {
        ColumnPositions cols;
        double leftmostBoldName = std::numeric_limits<double>::max();
        double minMemberFullNameX = std::numeric_limits<double>::max();

        for (const PdfTextField& field : fields)
        {
            QString text = field.text().trimmed();

            // Detect gender column
            if (Gender::isGender(text) && !cols.genderX)
            {
                cols.genderX = field.left();
            }

            // Detect birthday column
            if (Birthday::isBirthday(text) && !cols.birthdayX)
            {
                cols.birthdayX = field.left();
            }

            // Detect minister column (leftmost bold "Last, First")
            if (field.bold() && Name::looksLikeFullName(text) && field.left() < leftmostBoldName)
            {
                leftmostBoldName = field.left();
                cols.ministerX = field.left();
            }

            // Detect family column (bold text, not at minister position)
            // Works for both EQ (surname only) and RS (full "Last, First") formats
            if (field.bold() && !cols.familyX)
            {
                // Must be to the right of minister column if we have one
                if (!cols.ministerX || field.left() > *cols.ministerX + X_TOLERANCE)
                {
                    cols.familyX = field.left();
                    // Detect format: RS has full names, EQ has surnames only
                    cols.isRSFormat = Name::looksLikeFullName(text);
                }
            }

            // Track leftmost full name in member area (to the right of family header)
            // This will be parentNameX - parents are flush left, children are indented
            if (Name::looksLikeFullName(text)
                && cols.familyX
                && field.left() > *cols.familyX + X_TOLERANCE
                && field.left() < minMemberFullNameX)
            {
                minMemberFullNameX = field.left();
            }
        }

        // Validate parentNameX is in member range (left of gender column)
        if (cols.genderX && minMemberFullNameX < *cols.genderX - X_TOLERANCE)
        {
            cols.parentNameX = minMemberFullNameX;
        }

        return cols;
    }

    // ============================================================================
    // Position Check Helpers
    // ============================================================================

    bool isNearMinisterX(double x, const ColumnPositions& cols)
    {
        return cols.ministerX && std::abs(x - *cols.ministerX) <= X_TOLERANCE;
    }

    bool isNearFamilyX(double x, const ColumnPositions& cols)
    {
        return cols.familyX && std::abs(x - *cols.familyX) <= X_TOLERANCE;
    }

    // Helper to check if X is in the parent name range (at parentNameX, within threshold)
    bool isInParentNameRange(double x, const ColumnPositions& cols)
    {
        if (!cols.parentNameX)
        {
            return false;
        }
        // Parents are flush left at parentNameX, but must be to the right of family header
        double lowerBound = cols.familyX ? *cols.familyX + X_TOLERANCE : *cols.parentNameX - X_TOLERANCE;
        return x >= lowerBound && x < *cols.parentNameX + PARENT_CHILD_THRESHOLD;
    }

    // Helper to check if X is in the child name range (indented from parentNameX)
    bool isInChildNameRange(double x, const ColumnPositions& cols)
    {
        if (!cols.parentNameX || !cols.genderX)
        {
            return false;
        }
        // Children are indented more than PARENT_CHILD_THRESHOLD from parentNameX
        return x > *cols.parentNameX + PARENT_CHILD_THRESHOLD
            && x < *cols.genderX - X_TOLERANCE;
    }

    // Helper to check if X is in the member name range (either parent or child)
    bool isInMemberNameRange(double x, const ColumnPositions& cols)
    {
        return isInParentNameRange(x, cols) || isInChildNameRange(x, cols);
    }

    bool isNearGenderX(double x, const ColumnPositions& cols)
    {
        return cols.genderX && std::abs(x - *cols.genderX) <= X_TOLERANCE;
    }

    bool isNearBirthdayX(double x, const ColumnPositions& cols)
    {
        return cols.birthdayX && std::abs(x - *cols.birthdayX) <= X_TOLERANCE;
    }

    // ============================================================================
    // Contact Info Parsing
    // ============================================================================

    /// Parse contact info from text fields, applying directly to person and family.
    /// Phone and email are set on person; address is set on family.
    /// Skips first field (the name) by default.
    void parseContactInfo(const QList<PdfTextField>& fields, Person& person, Family& family, int startIndex = 1)
    {
        Address address;
        for (int i = startIndex; i < fields.size(); ++i)
        {
            QString t = fields[i].text().trimmed();
            if (t.isEmpty())
            {
                continue;
            }
            if (person.phone().isEmpty() && Phone::isPhone(t))
            {
                person.setPhone(Phone(t));
            }
            else if (person.email().isEmpty() && Email::isEmail(t))
            {
                person.setEmail(Email(t));
            }
            else
            {
                address.addLine(t);
            }
        }
        family.setAddress(address);
    }

    // ============================================================================
    // Header Parsing
    // ============================================================================

    void parseHeaderInfo(const QString& unitText, QString& name, QString& unitNumber)
    {
        int parenPos = unitText.lastIndexOf('(');
        if (parenPos >= 0 && unitText.endsWith(')'))
        {
            name = unitText.left(parenPos).trimmed();
            unitNumber = unitText.mid(parenPos + 1, unitText.length() - parenPos - 2).trimmed();
        }
        else
        {
            name = unitText.trimmed();
        }
    }

    // Parse date from footer like "21 Sep 2025"
    QDate parseFooterDate(const QString& text)
    {
        QRegularExpressionMatch match = DATE_PATTERN.match(text.trimmed());
        if (match.hasMatch())
        {
            int day = match.captured(1).toInt();
            int month = MONTH_MAP.value(match.captured(2), 0);
            int year = match.captured(3).toInt();
            if (month > 0)
            {
                return QDate(year, month, day);
            }
        }
        return QDate();
    }

    // ============================================================================
    // Minister Parsing - Creates Family objects with minister as member
    // ============================================================================

    /// Parse a single minister from their fields.
    /// Returns Person and Family for this minister.
    std::pair<Person, Family> parseOneMinister(
        const QList<PdfTextField>& ministerFields,
        Gender ministerGender)
    {
        Name name(ministerFields[0].text().trimmed());

        Person minister = Person::create(
            name,
            false,     // isParent (unknown - will be fixed on merge)
            Phone(),   // phone (filled by parseContactInfo)
            Phone(),   // altPhone
            Email(),   // email (filled by parseContactInfo)
            ministerGender,
            Birthday()
        );

        Family family = Family::create(
            std::nullopt,  // latitude
            std::nullopt   // longitude
            // address filled by parseContactInfo, members set after
        );

        parseContactInfo(ministerFields, minister, family);
        family.setMembers({minister});

        return {minister, family};
    }

    /// Process pending minister fields, adding to collections.
    void processPendingMinister(
        QList<PdfTextField>& pending,
        QHash<QString, Family>& ministerFamilies,
        QList<Person>& ministers,
        Gender ministerGender)
    {
        if (pending.isEmpty())
        {
            return;
        }
        std::pair<Person, Family> parsed = parseOneMinister(pending, ministerGender);
        Person minister = parsed.first;
        Family family = parsed.second;
        ministerFamilies.insert(family.id(), family);
        ministers.append(minister);
        pending.clear();
    }

    /// Parse ministers from fields, creating Family objects with minister as member.
    /// Returns list of Person objects (the ministers) for this companionship.
    /// Also inserts minister families into the provided hash.
    QList<Person> parseMinistersFromFields(
        const QList<PdfTextField>& fields,
        QHash<QString, Family>& ministerFamilies,
        Gender ministerGender)
    {
        QList<Person> companionshipMinisters;
        QList<PdfTextField> currentMinisterFields;

        for (const PdfTextField& field : fields)
        {
            if (field.bold() && Name::looksLikeFullName(field.text().trimmed()))
            {
                processPendingMinister(currentMinisterFields, ministerFamilies,
                                       companionshipMinisters, ministerGender);
            }
            currentMinisterFields.append(field);
        }

        processPendingMinister(currentMinisterFields, ministerFamilies,
                               companionshipMinisters, ministerGender);

        return companionshipMinisters;
    }

    // ============================================================================
    // Family Member Parsing - Creates Person objects directly
    // ============================================================================

    /// Parse members from member text fields grouped by Y position.
    /// Creates Person objects from full "Surname, GivenNames" format.
    QList<Person> parseFamilyMembers(
        const QList<PdfTextField>& memberFields,
        const ColumnPositions& cols,
        const QDate& documentDate)
    {
        QList<Person> members;
        QList<QList<PdfTextField>> memberRows = groupFieldsByY(memberFields, Y_GROUP_TOLERANCE);

        for (const QList<PdfTextField>& row : memberRows)
        {
            QString nameText;
            double nameX = 0;
            QString genderText;
            QString birthdayText;

            for (const PdfTextField& field : row)
            {
                if (isNearBirthdayX(field.left(), cols))
                {
                    birthdayText = field.text().trimmed();
                }
                else if (isNearGenderX(field.left(), cols))
                {
                    genderText = field.text().trimmed();
                }
                else if (isInMemberNameRange(field.left(), cols))
                {
                    if (nameText.isEmpty())
                    {
                        nameText = field.text().trimmed();
                        nameX = field.left();
                    }
                }
            }

            if (nameText.isEmpty())
            {
                continue;
            }

            // Member name is full "Surname, GivenNames" format
            Name name(nameText);

            std::optional<Gender> gender = Gender::fromString(genderText);
            Birthday birthday = parseBirthday(birthdayText, documentDate);

            Person member = Person::create(
                name,
                isInParentNameRange(nameX, cols),
                Phone(),   // phone
                Phone(),   // altPhone
                Email(),   // email
                gender,
                birthday
            );
            members.append(member);
        }

        return members;
    }

    // ============================================================================
    // Family Parsing - Creates Family objects directly
    // ============================================================================

    /// Result of parsing a single family's fields
    struct ParsedFamily
    {
        Family family;
        std::optional<QString> ministeredPersonId;  // For RS format
        QString error;
        bool isRSFormat = false;  // Detected format (only meaningful for first family)
    };

    /// Parse a single family from its fields.
    ParsedFamily parseOneFamily(
        const QList<PdfTextField>& familyFields,
        const ColumnPositions& cols,
        const QDate& documentDate)
    {
        ParsedFamily result;

        // Separate header fields from member fields
        QList<PdfTextField> headerFields;
        QList<PdfTextField> memberFields;

        for (const PdfTextField& field : familyFields)
        {
            if (isInMemberNameRange(field.left(), cols)
                || isNearGenderX(field.left(), cols)
                || isNearBirthdayX(field.left(), cols))
            {
                memberFields.append(field);
            }
            else
            {
                headerFields.append(field);
            }
        }

        if (headerFields.isEmpty())
        {
            // No family header found - skip this group
            return result;  // family.id() will be empty, caller should skip
        }

        // Concatenate bold fields at family X position (handles wrapped names)
        QString headerText = concatenateMatchingFields(headerFields, [&](const PdfTextField& f) {
            return f.bold() && isNearFamilyX(f.left(), cols);
        });
        if (headerText.isEmpty())
        {
            headerText = headerFields[0].text().trimmed();
        }

        QString surname;
        std::optional<QString> ministeredPersonName;

        // Detect EQ vs RS based on header format
        result.isRSFormat = Name::looksLikeFullName(headerText);

        if (Name::isSurnameOnly(headerText))
        {
            // EQ style: just surname
            surname = headerText;
        }
        else if (Name::looksLikeFullName(headerText))
        {
            // RS style: full "Last, First" name
            ministeredPersonName = headerText;
            int commaPos = headerText.indexOf(',');
            surname = (commaPos > 0) ? headerText.left(commaPos).trimmed() : headerText;
        }
        else
        {
            // Unknown format, use as-is
            surname = headerText;
        }

        QList<Person> members = parseFamilyMembers(memberFields, cols, documentDate);

        // Find target member for contact info and (for RS) ministered person ID
        int targetIndex = 0;  // EQ: first member (head of family)

        if (ministeredPersonName.has_value() && !members.isEmpty())
        {
            // RS: find ministered person by name
            Name ministeredName(*ministeredPersonName);
            bool found = false;
            for (int i = 0; i < members.size(); ++i)
            {
                if (members[i].name() == ministeredName)
                {
                    targetIndex = i;
                    result.ministeredPersonId = members[i].id();
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                result.error = QString("RS ministered person '%1' not found in family members")
                    .arg(*ministeredPersonName);
                return result;
            }
        }

        // Create family (address will be set by parseContactInfo)
        result.family = Family::create(
            std::nullopt,  // latitude
            std::nullopt   // longitude
        );

        // Apply contact info: phone/email to target member, address to family
        if (!members.isEmpty())
        {
            parseContactInfo(headerFields, members[targetIndex], result.family);
        }

        result.family.setMembers(members);

        return result;
    }

    /// Split fields into groups, one per family (delimited by bold family headers).
    QList<QList<PdfTextField>> splitByFamilyHeader(
        const QList<PdfTextField>& fields,
        const ColumnPositions& cols)
    {
        QList<QList<PdfTextField>> groups;
        QList<PdfTextField> current;

        for (const PdfTextField& field : fields)
        {
            bool isHeader = field.bold() && isNearFamilyX(field.left(), cols);
            if (isHeader && !current.isEmpty())
            {
                groups.append(current);
                current.clear();
            }
            current.append(field);
        }
        if (!current.isEmpty())
        {
            groups.append(current);
        }

        return groups;
    }

    /// Parse families from fields, creating Family and Person objects.
    /// Returns ministered IDs: family IDs for EQ format, person IDs for RS format.
    /// Inserts families into ministeredFamilies hash.
    /// Sets errorOut on failure (returns empty set).
    QSet<QString> parseFamiliesFromFields(
        const QList<PdfTextField>& fields,
        const ColumnPositions& cols,
        const QDate& documentDate,
        QHash<QString, Family>& ministeredFamilies,
        QString& errorOut)
    {
        QSet<QString> ministeredIds;

        for (const QList<PdfTextField>& group : splitByFamilyHeader(fields, cols))
        {
            ParsedFamily parsed = parseOneFamily(group, cols, documentDate);
            if (!parsed.error.isEmpty())
            {
                errorOut = parsed.error;
                return {};
            }

            // Skip invalid families (e.g., empty header groups)
            if (parsed.family.id().isEmpty())
            {
                continue;
            }

            ministeredFamilies.insert(parsed.family.id(), parsed.family);

            // EQ: track family IDs; RS: track ministered person IDs
            if (parsed.ministeredPersonId.has_value())
            {
                ministeredIds.insert(*parsed.ministeredPersonId);
            }
            else
            {
                ministeredIds.insert(parsed.family.id());
            }
        }

        return ministeredIds;
    }

    // ============================================================================
    // Companionship Parsing
    // ============================================================================

    /// Parse accumulated companionship fields and create ministering group.
    /// Creates MinisteringGroup with familyIds (EQ) or ministeredPersonIds (RS).
    /// Returns error message on failure, empty string on success.
    QString parseCompanionship(
        QList<PdfTextField>& companionshipFields,
        QString& presidencyName,
        const QString& currentDistrictId,
        MinisteringPdfParser::ParseResult& result)
    {
        if (companionshipFields.isEmpty() || currentDistrictId.isEmpty())
        {
            return QString();
        }

        // Detect column positions from content
        ColumnPositions cols = detectColumnPositions(companionshipFields);

        // Validate: if we have families, we must have gender and birthday columns
        if (cols.familyX)
        {
            if (!cols.genderX)
            {
                return "Companionship parsing failed: family detected but no gender column found. "
                       "PDF format may have changed.";
            }
            if (!cols.birthdayX)
            {
                return "Companionship parsing failed: family detected but no birthday column found. "
                       "PDF format may have changed.";
            }
        }

        // Separate minister fields from family fields using detected positions
        QList<PdfTextField> ministerFields;
        QList<PdfTextField> familyFields;

        for (const PdfTextField& field : companionshipFields)
        {
            if (isNearMinisterX(field.left(), cols))
            {
                ministerFields.append(field);
            }
            else if (cols.ministerX && field.left() > *cols.ministerX + X_TOLERANCE)
            {
                familyFields.append(field);
            }
        }

        // Detect format when we first see a family header
        // This is authoritative - RS has "Last, First" headers, EQ has surname only
        if (cols.familyX.has_value() && !result.detectedFormat.has_value())
        {
            result.detectedFormat = cols.isRSFormat;

            // If we just detected RS format, backfill earlier groups and ministers
            if (cols.isRSFormat)
            {
                // Convert earlier groups from EQ to RS
                for (auto& group : result.groups)
                {
                    group.setIsRSGroup(true);
                }

                // Fix minister genders from Male to Female
                for (auto& family : result.ministerFamilies)
                {
                    QList<Person> members = family.members();
                    for (Person& member : members)
                    {
                        if (member.gender() == Gender::Male)
                        {
                            member.setGender(Gender::Female);
                        }
                    }
                    family.setMembers(members);
                }
            }
        }

        // Use detected format if known, otherwise use column detection for this companionship
        bool effectiveRSFormat = result.detectedFormat.value_or(cols.isRSFormat);
        result.isRSFormat = effectiveRSFormat;

        // Parse ministers into Family objects (with minister as member)
        Gender ministerGender = effectiveRSFormat ? Gender::Female : Gender::Male;
        QList<Person> ministers = parseMinistersFromFields(ministerFields, result.ministerFamilies, ministerGender);

        // Merge wrapped bold family headers before parsing
        if (cols.familyX)
        {
            mergeWrappedBoldFields(familyFields, *cols.familyX);
        }

        // Parse families - returns family IDs (EQ) or ministered person IDs (RS)
        QSet<QString> ministeredIds;
        if (!familyFields.isEmpty())
        {
            QString familyError;
            ministeredIds = parseFamiliesFromFields(
                familyFields, cols, result.documentDate, result.ministeredFamilies, familyError);

            if (!familyError.isEmpty())
            {
                return familyError;
            }
        }

        // Build minister ID set and find presidency member if specified
        QSet<QString> ministerIds;
        std::optional<QString> presidencyMemberId;
        for (const Person& minister : ministers)
        {
            ministerIds.insert(minister.id());

            // Check if this minister is the presidency member
            if (!presidencyName.isEmpty() && minister.name() == Name(presidencyName))
            {
                presidencyMemberId = minister.id();
            }
        }

        // Create group: EQ uses familyIds, RS uses ministeredPersonIds
        MinisteringGroup group = result.isRSFormat
            ? MinisteringGroup::createRS(ministerIds, ministeredIds)
            : MinisteringGroup::createEQ(ministerIds, ministeredIds);
        group.setPresidencyMemberId(presidencyMemberId);
        result.groups.insert(group.id(), group);

        // Add group to current district
        result.districts[currentDistrictId].addGroup(group.id());

        companionshipFields.clear();
        presidencyName.clear();
        return QString();
    }

}  // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

MinisteringPdfParser::ParseResult MinisteringPdfParser::parse(const QString& pdfPath)
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

    QList<PdfTextField> allContentFields;
    double yOffset = 0.0;

    for (int pageNum = 0; pageNum < numPages; ++pageNum)
    {
        QList<PdfTextField> pageFields = extractor.textFields(pageNum);
        if (pageFields.isEmpty())
        {
            continue;
        }

        // Find content boundaries (between header and footer)
        // Header: "Ministering Assignments" + ward + stake (2 fields after marker)
        // Footer: page number (1 field before marker) + "For Church Use Only"
        std::pair<int, int> contentBounds = findContentBounds(
            pageFields,
            [](const QString& text) { return text == "Ministering Assignments"; },
            [](const QString& text) { return text.contains("For Church Use Only"); },
            2,  // headerFieldsAfterMarker: ward, stake
            1); // footerFieldsBeforeMarker: page number
        int contentStart = contentBounds.first;
        int contentEnd = contentBounds.second;

        // Check if markers were found (contentStart=0 means no header, contentEnd=size means no footer)
        if (contentStart == 0 || contentEnd == pageFields.size())
        {
            result.errors.append(QString("Page %1: Unrecognized PDF format - missing header or footer markers")
                .arg(pageNum + 1));
            return result;
        }

        // Extract metadata from first page header/footer
        if (pageNum == 0)
        {
            // Find header start to locate ward/stake lines
            for (int i = 0; i < pageFields.size(); ++i)
            {
                if (pageFields[i].text().trimmed() == "Ministering Assignments")
                {
                    if (i + 2 >= pageFields.size())
                    {
                        result.errors.append("PDF header truncated: missing ward/stake info after 'Ministering Assignments'");
                        return result;
                    }
                    parseHeaderInfo(pageFields[i + 1].text(), result.wardName, result.wardUnitNumber);
                    parseHeaderInfo(pageFields[i + 2].text(), result.stakeName, result.stakeUnitNumber);
                    break;
                }
            }

            // Find date in footer
            for (int i = contentEnd; i < pageFields.size(); ++i)
            {
                QDate date = parseFooterDate(pageFields[i].text());
                if (date.isValid())
                {
                    result.documentDate = date;
                    break;
                }
            }
        }

        // Append content fields (filtered - no headers/footers).
        // Apply cumulative Y offset so that Y coordinates are document-relative
        // rather than page-relative. This ensures Y-based grouping and merging
        // works correctly when companionships span multiple pages.
        for (int i = contentStart; i < contentEnd; ++i)
        {
            pageFields[i].addY(yOffset);
            allContentFields.append(pageFields[i]);
        }

        // Accumulate offset for next page: footer Y (beyond all content) plus
        // header Y (content start position) provides a natural page gap.
        if (contentEnd < pageFields.size())
        {
            yOffset += pageFields[contentEnd].y() + pageFields[contentStart].y();
        }
    }

    if (allContentFields.isEmpty())
    {
        result.errors.append("No content found in PDF");
        return result;
    }

    if (!result.documentDate.isValid())
    {
        result.errors.append("Could not parse document date from PDF footer");
        return result;
    }

    // ========================================================================
    // Phase 2: Parse content fields - create model objects directly
    // ========================================================================

    float districtFontSize = detectDistrictFontSize(allContentFields);

    // Group all content by Y position
    QList<QList<PdfTextField>> fieldRows = groupFieldsByY(allContentFields, Y_GROUP_TOLERANCE);

    // Merge consecutive district-title rows (handles wrapped titles)
    for (int i = 1; i < fieldRows.size(); ++i)
    {
        if (isDistrictTitle(fieldRows[i - 1][0].fontSize(), districtFontSize)
            && isDistrictTitle(fieldRows[i][0].fontSize(), districtFontSize))
        {
            fieldRows[i - 1].append(fieldRows[i]);
            fieldRows.removeAt(i);
            --i;
        }
    }

    // Parsing state
    QList<PdfTextField> currentCompanionshipFields;
    QString currentPresidencyName;
    QString currentDistrictId;

    for (const QList<PdfTextField>& row : fieldRows)
    {
        if (row.isEmpty())
        {
            continue;
        }

        const PdfTextField& firstField = row[0];

        // Check for dot separator (end of companionship)
        if (isDotSeparatorLine(row))
        {
            QString error = parseCompanionship(
                currentCompanionshipFields, currentPresidencyName, currentDistrictId, result);
            if (!error.isEmpty())
            {
                result.errors.append(error);
                return result;
            }
            continue;
        }

        // Check for district title (largest font)
        if (isDistrictTitle(firstField.fontSize(), districtFontSize))
        {
            // Finalize previous companionship
            QString error = parseCompanionship(
                currentCompanionshipFields, currentPresidencyName, currentDistrictId, result);
            if (!error.isEmpty())
            {
                result.errors.append(error);
                return result;
            }

            // Collect all fields in this row as district title
            QString districtName = concatenateMatchingFields(row, [](const PdfTextField&) { return true; });

            // Create new district
            MinisteringDistrict district = MinisteringDistrict::create(districtName);
            currentDistrictId = district.id();
            result.districts.insert(currentDistrictId, district);
            continue;
        }

        // Check for "Presidency Member:" label
        bool isPresidencyRow = false;
        for (const PdfTextField& field : row)
        {
            if (isPresidencyMemberLabel(field.text()))
            {
                isPresidencyRow = true;
                // Find the bold name on the same row
                for (const PdfTextField& other : row)
                {
                    if (other.bold() && Name::looksLikeFullName(other.text().trimmed()))
                    {
                        QString presidencyMemberName = other.text().trimmed();
                        // Use positional context: if no companionship fields accumulated yet,
                        // this is the district-level presidency member (appears right after
                        // district title). Otherwise it's companionship-level.
                        if (!currentDistrictId.isEmpty() && currentCompanionshipFields.isEmpty())
                        {
                            // District-level presidency member - create Family with Person
                            Name name(presidencyMemberName);
                            Person presidencyMember = Person::create(
                                name,
                                true,      // isParent (presidency members are adults)
                                Phone(),   // phone
                                Phone(),   // altPhone
                                Email(),   // email
                                Gender::Male,  // EQ presidency members are male
                                Birthday()
                            );

                            Family family = Family::create(
                                std::nullopt,
                                std::nullopt,
                                Address(),
                                {presidencyMember}
                            );

                            result.ministerFamilies.insert(family.id(), family);
                            result.districts[currentDistrictId].setPresidencyMemberId(presidencyMember.id());
                        }
                        else
                        {
                            currentPresidencyName = presidencyMemberName;
                        }
                        break;
                    }
                }
                break;
            }
        }
        if (isPresidencyRow)
        {
            continue;
        }

        // Accumulate fields for current companionship
        if (!currentDistrictId.isEmpty())
        {
            for (const PdfTextField& field : row)
            {
                currentCompanionshipFields.append(field);
            }
        }
    }

    // Finalize last companionship
    QString error = parseCompanionship(
        currentCompanionshipFields, currentPresidencyName, currentDistrictId, result);
    if (!error.isEmpty())
    {
        result.errors.append(error);
        return result;
    }

    result.success = true;
    return result;
}
