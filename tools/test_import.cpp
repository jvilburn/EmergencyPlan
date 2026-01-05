// Test tool for PDF import services
// Usage: test_import -w <pdf_file>
//        test_import -e <pdf_file>

#include "WardDirectoryImportService.h"
#include "WardDirectoryPdfParser.h"
#include "MinisteringImportService.h"
#include "MinisteringPdfParser.h"
#include "MinisteringGroup.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>

enum class ImportMode
{
    None,
    WardDirectory,
    EQMinistering
};

void usage(const char* progName)
{
    QTextStream err(stderr);
    err << "Usage: " << progName << " <mode> [options] <pdf_file>\n"
        << "\n"
        << "Modes (required):\n"
        << "  -w, --ward   Import ward directory PDF\n"
        << "  -e, --eq     Import EQ ministering PDF\n"
        << "\n"
        << "Options:\n"
        << "  -r, --raw    Use parser directly (skip import service)\n"
        << "  -f <name>    Filter output by name (case-insensitive)\n"
        << "  -h, --help   Show this help\n"
        << "\n"
        << "Examples:\n"
        << "  test_import -w directory.pdf\n"
        << "  test_import -w -r directory.pdf\n"
        << "  test_import -e ministering.pdf\n";
}

// ============================================================================
// Common Helpers
// ============================================================================

void dumpPerson(const Person& p, QTextStream& out, const QString& indent)
{
    out << indent << "ID: " << p.id() << "\n";
    out << indent << "Name: " << p.displayName() << "\n";
    out << indent << "Given Names: " << p.givenNames() << "\n";
    out << indent << "Surname: " << p.surname() << "\n";
    if (!p.phone().isEmpty())
    {
        out << indent << "Phone: " << p.phone() << "\n";
    }
    if (!p.altPhone().isEmpty())
    {
        out << indent << "Alt Phone: " << p.altPhone() << "\n";
    }
    if (!p.email().isEmpty())
    {
        out << indent << "Email: " << p.email() << "\n";
    }
    if (p.gender().has_value())
    {
        out << indent << "Gender: " << p.gender()->toString() << "\n";
    }
    if (p.birthday().hasDate())
    {
        out << indent << "Birth: " << p.birthDateDisplay() << "\n";
    }
    if (!p.callings().isEmpty())
    {
        out << indent << "Callings: " << p.callings().join(", ") << "\n";
    }
}

void dumpFamily(const Family& f, QTextStream& out, const QString& indent)
{
    out << indent << "ID: " << f.id() << "\n";
    out << indent << "Display Name: " << f.displayName() << "\n";
    if (!f.address().isEmpty())
    {
        out << indent << "Address: " << f.address().full() << "\n";
    }
    if (f.isMapped())
    {
        out << indent << "Location: " << *f.latitude() << ", " << *f.longitude() << "\n";
    }
    out << indent << "Members (" << f.members().size() << "):\n";
    for (const Person& p : f.members())
    {
        out << indent << "  ----\n";
        dumpPerson(p, out, indent + "  ");
    }
}

// ============================================================================
// Ward Directory
// ============================================================================

int importWardDirectory(const QString& pdfPath, const QString& filter, QTextStream& out, QTextStream& err)
{
    WardDirectoryImportService service;

    out << "Importing ward directory (service): " << pdfPath << "\n\n";
    out.flush();

    WardDirectoryImportResult result = service.importFromPdf(pdfPath);

    if (!result.success)
    {
        err << "ERROR: Import failed\n";
        for (const QString& error : result.errors)
        {
            err << "  " << error << "\n";
        }
        return 1;
    }

    out << "Ward: " << result.wardName << " (" << result.wardUnitNumber << ")\n";
    out << "Total families: " << result.families.size() << "\n\n";

    for (auto it = result.families.constBegin(); it != result.families.constEnd(); ++it)
    {
        const Family& family = it.value();
        if (!filter.isEmpty() && !family.displayName().contains(filter, Qt::CaseInsensitive))
        {
            continue;
        }

        out << "----------------------------------------\n";
        dumpFamily(family, out, "");
        out << "\n";
    }

    out << "Import completed successfully.\n";
    return 0;
}

int parseWardDirectory(const QString& pdfPath, const QString& filter, QTextStream& out, QTextStream& err)
{
    out << "Parsing ward directory (raw): " << pdfPath << "\n\n";
    out.flush();

    WardDirectoryPdfParser::ParseResult result = WardDirectoryPdfParser::parse(pdfPath);

    if (!result.success)
    {
        err << "ERROR: Parse failed\n";
        for (const QString& error : result.errors)
        {
            err << "  " << error << "\n";
        }
        return 1;
    }

    out << "Ward: " << result.wardName << " (" << result.wardUnitNumber << ")\n";
    out << "Total families: " << result.families.size() << "\n\n";

    for (auto it = result.families.constBegin(); it != result.families.constEnd(); ++it)
    {
        const Family& family = it.value();
        if (!filter.isEmpty() && !family.displayName().contains(filter, Qt::CaseInsensitive))
        {
            continue;
        }

        out << "----------------------------------------\n";
        dumpFamily(family, out, "");
        out << "\n";
    }

    out << "Parse completed successfully.\n";
    return 0;
}

// ============================================================================
// EQ Ministering
// ============================================================================

int importEQMinistering(const QString& pdfPath, QTextStream& out, QTextStream& err)
{
    out << "Importing ministering (service): " << pdfPath << "\n\n";
    out.flush();

    MinisteringImportService service;
    QHash<QString, Family> existing;
    MinisteringImportResult result = service.importFromPdf(pdfPath, existing);

    if (!result.success)
    {
        err << "ERROR: Import failed\n";
        for (const QString& error : result.errors)
        {
            err << "  " << error << "\n";
        }
        return 1;
    }

    QString orgType = result.isRSFormat ? "RS" : "EQ";
    int groupCount = result.groups.size();

    out << "Format: " << orgType << "\n";
    out << "Ward: " << result.wardName << " (" << result.wardUnitNumber << ")\n";
    out << "Stake: " << result.stakeName << " (" << result.stakeUnitNumber << ")\n";
    out << "Districts: " << result.districts.size() << "\n";
    out << "Groups: " << groupCount << "\n";
    out << "Families: " << result.families.size() << "\n\n";

    // Build lookup maps
    QHash<QString, QString> personToFamilyMap;  // personId -> familyId
    for (auto it = result.families.constBegin(); it != result.families.constEnd(); ++it)
    {
        for (const Person& p : it.value().members())
        {
            personToFamilyMap.insert(p.id(), it.key());
        }
    }

    // Dump all districts with their groups
    for (const MinisteringDistrict& district : result.districts)
    {
        out << "################################################################################\n";
        out << "DISTRICT: " << district.name() << "\n";
        out << "ID: " << district.id() << "\n";
        if (district.presidencyMemberId().has_value())
        {
            out << "Presidency Member ID: " << district.presidencyMemberId().value() << "\n";
        }
        out << "################################################################################\n\n";

        // Dump each group in this district
        for (const QString& groupId : district.groupIds())
        {
            auto groupIt = result.groups.find(groupId);
            if (groupIt == result.groups.end())
            {
                out << "  [Group " << groupId << " not found!]\n\n";
                continue;
            }

            const MinisteringGroup& group = *groupIt;
            out << "  ========================================\n";
            out << "  " << orgType << " GROUP ID: " << group.id() << "\n";
            if (group.interviewedDate().has_value())
            {
                out << "  Interviewed: " << group.interviewedDate()->toString("yyyy-MM-dd") << "\n";
            }
            out << "  ========================================\n";

            out << "  MINISTERS (" << group.ministerIds().size() << "):\n";
            for (const QString& ministerId : group.ministerIds())
            {
                out << "    ----------------------------------------\n";
                out << "    Person ID: " << ministerId << "\n";
                auto famIdIt = personToFamilyMap.find(ministerId);
                if (famIdIt != personToFamilyMap.end())
                {
                    auto famIt = result.families.find(*famIdIt);
                    if (famIt != result.families.end())
                    {
                        dumpFamily(*famIt, out, "    ");
                    }
                }
                else
                {
                    out << "    [NOT FOUND - person not in any family]\n";
                }
            }

            if (result.isRSFormat)
            {
                out << "  MINISTERED PERSONS (" << group.ministeredPersonIds().size() << "):\n";
                for (const QString& personId : group.ministeredPersonIds())
                {
                    out << "    Person ID: " << personId << "\n";
                }
            }
            else
            {
                out << "  MINISTERED FAMILIES (" << group.familyIds().size() << "):\n";
                for (const QString& famId : group.familyIds())
                {
                    out << "    ----------------------------------------\n";
                    auto famIt = result.families.find(famId);
                    if (famIt != result.families.end())
                    {
                        dumpFamily(*famIt, out, "    ");
                    }
                    else
                    {
                        out << "    Family ID: " << famId << " [NOT FOUND]\n";
                    }
                }
            }
            out << "\n";
        }
    }

    // Dump all families for reference
    out << "\n################################################################################\n";
    out << "ALL FAMILIES (" << result.families.size() << ")\n";
    out << "################################################################################\n\n";
    for (const Family& family : result.families)
    {
        out << "----------------------------------------\n";
        dumpFamily(family, out, "");
        out << "\n";
    }

    out << "Import completed successfully.\n";
    return 0;
}

int parseEQMinistering(const QString& pdfPath, QTextStream& out, QTextStream& err)
{
    out << "Parsing ministering (raw): " << pdfPath << "\n\n";
    out.flush();

    MinisteringPdfParser::ParseResult parseResult = MinisteringPdfParser::parse(pdfPath);

    if (!parseResult.success)
    {
        err << "ERROR: Parse failed\n";
        for (const QString& error : parseResult.errors)
        {
            err << "  " << error << "\n";
        }
        return 1;
    }

    QString orgType = parseResult.isRSFormat ? "RS" : "EQ";
    int groupCount = parseResult.groups.size();

    int totalFamilies = parseResult.ministerFamilies.size() + parseResult.ministeredFamilies.size();
    out << "Format: " << orgType << "\n";
    out << "Ward: " << parseResult.wardName << " (" << parseResult.wardUnitNumber << ")\n";
    out << "Stake: " << parseResult.stakeName << " (" << parseResult.stakeUnitNumber << ")\n";
    out << "Districts: " << parseResult.districts.size() << "\n";
    out << "Groups: " << groupCount << "\n";
    out << "Minister Families: " << parseResult.ministerFamilies.size() << "\n";
    out << "Ministered Families: " << parseResult.ministeredFamilies.size() << "\n";
    out << "Total Families: " << totalFamilies << "\n\n";

    // Build lookup maps (combining minister and ministered families)
    QHash<QString, Family> familyMap;
    QHash<QString, QString> personToFamilyMap;
    for (auto it = parseResult.ministerFamilies.constBegin(); it != parseResult.ministerFamilies.constEnd(); ++it)
    {
        familyMap.insert(it.key(), it.value());
        for (const Person& p : it.value().members())
        {
            personToFamilyMap.insert(p.id(), it.key());
        }
    }
    for (auto it = parseResult.ministeredFamilies.constBegin(); it != parseResult.ministeredFamilies.constEnd(); ++it)
    {
        familyMap.insert(it.key(), it.value());
        for (const Person& p : it.value().members())
        {
            personToFamilyMap.insert(p.id(), it.key());
        }
    }

    for (auto it = parseResult.districts.constBegin(); it != parseResult.districts.constEnd(); ++it)
    {
        const MinisteringDistrict& district = it.value();
        out << "========================================\n";
        out << "DISTRICT: " << district.name() << "\n";
        out << "ID: " << district.id() << "\n";
        if (district.presidencyMemberId().has_value())
        {
            out << "Presidency Member ID: " << district.presidencyMemberId().value() << "\n";
        }
        out << "Groups: " << district.groupIds().size() << "\n";
        out << "========================================\n\n";

        int groupNum = 1;
        for (const QString& groupId : district.groupIds())
        {
            out << "--- Group " << groupNum++ << " ---\n";

            auto groupIt = parseResult.groups.find(groupId);
            if (groupIt == parseResult.groups.end())
            {
                out << "  [Group not found]\n\n";
                continue;
            }
            const MinisteringGroup& group = *groupIt;

            out << "Ministers:\n";
            for (const QString& ministerId : group.ministerIds())
            {
                auto famId = personToFamilyMap.value(ministerId);
                auto family = familyMap.value(famId);
                for (const Person& p : family.members())
                {
                    if (p.id() == ministerId)
                    {
                        out << "  " << p.name().full() << "\n";
                        if (!p.displayPhone().isEmpty())
                        {
                            out << "    Phone: " << p.displayPhone() << "\n";
                        }
                        if (!p.email().isEmpty())
                        {
                            out << "    Email: " << p.email() << "\n";
                        }
                        break;
                    }
                }
            }

            if (parseResult.isRSFormat)
            {
                out << "Ministered Persons:\n";
                for (const QString& personId : group.ministeredPersonIds())
                {
                    auto famId = personToFamilyMap.value(personId);
                    auto family = familyMap.value(famId);
                    for (const Person& p : family.members())
                    {
                        if (p.id() == personId)
                        {
                            out << "  " << p.name().full() << "\n";
                            break;
                        }
                    }
                }
            }
            else if (!group.familyIds().isEmpty())
            {
                out << "Ministered Families:\n";
                for (const QString& famId : group.familyIds())
                {
                    auto family = familyMap.value(famId);
                    out << "  " << family.displayName() << "\n";
                    if (!family.displayPhone().isEmpty())
                    {
                        out << "    Phone: " << family.displayPhone() << "\n";
                    }
                    if (!family.members().isEmpty())
                    {
                        out << "    Members:\n";
                        for (const Person& m : family.members())
                        {
                            out << "      " << m.name().full();
                            if (m.gender().has_value())
                            {
                                out << " (" << m.gender()->toString() << ")";
                            }
                            if (m.birthday().hasDate())
                            {
                                out << " - " << m.birthday().dateDisplay();
                            }
                            out << "\n";
                        }
                    }
                }
            }
            out << "\n";
        }
    }

    out << "Parse completed successfully.\n";
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QString pdfPath;
    QString filter;
    ImportMode mode = ImportMode::None;
    bool rawMode = false;

    for (int i = 1; i < argc; ++i)
    {
        QString arg = QString::fromLocal8Bit(argv[i]);

        if (arg == "-h" || arg == "--help")
        {
            usage(argv[0]);
            return 0;
        }
        else if (arg == "-w" || arg == "--ward")
        {
            mode = ImportMode::WardDirectory;
        }
        else if (arg == "-e" || arg == "--eq")
        {
            mode = ImportMode::EQMinistering;
        }
        else if (arg == "-r" || arg == "--raw")
        {
            rawMode = true;
        }
        else if (arg == "-f")
        {
            if (i + 1 >= argc)
            {
                qWarning() << "ERROR: -f requires a name";
                return 1;
            }
            filter = QString::fromLocal8Bit(argv[++i]);
        }
        else if (arg.startsWith("-"))
        {
            qWarning() << "ERROR: Unknown option:" << arg;
            usage(argv[0]);
            return 1;
        }
        else
        {
            pdfPath = arg;
        }
    }

    if (mode == ImportMode::None || pdfPath.isEmpty())
    {
        usage(argv[0]);
        return 1;
    }

    QTextStream out(stdout);
    QTextStream err(stderr);
    out.setEncoding(QStringConverter::Utf8);
    err.setEncoding(QStringConverter::Utf8);

    if (mode == ImportMode::WardDirectory)
    {
        if (rawMode)
        {
            return parseWardDirectory(pdfPath, filter, out, err);
        }
        return importWardDirectory(pdfPath, filter, out, err);
    }
    else
    {
        if (rawMode)
        {
            return parseEQMinistering(pdfPath, out, err);
        }
        return importEQMinistering(pdfPath, out, err);
    }
}
