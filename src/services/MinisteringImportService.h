#pragma once

#include "Family.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"

#include <QDate>
#include <QObject>
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

/// Service for importing ministering data from PDF files.
/// Handles ID resolution and merging with existing Document data.
class MinisteringImportService : public QObject
{
    Q_OBJECT

public:
    explicit MinisteringImportService(QObject* parent);
    ~MinisteringImportService();

    /// Import ministering assignments from a PDF file (auto-detects EQ or RS).
    /// If wardDirectoryDate is provided and is newer than the PDF date,
    /// family data from the PDF is treated as stale (only fills empty fields).
    MinisteringImportResult importFromPdf(
        const QString& pdfPath,
        const QHash<FamilyId, Family>& existingFamilies,
        std::optional<QDate> wardDirectoryDate);

private:
    /// Merge source families into target, remapping IDs in groups and districts.
    /// - Matches families by surname, displayName, address, phone
    /// - Matches persons by firstName, isParent
    /// - isParent=true wins over isParent=false when merging
    /// - When isAuthoritative: unmatched families are added, names can be updated
    /// - When not authoritative: only fills empty fields, no new families added
    void mergeFamilies(
        QHash<FamilyId, Family>& targetFamilies,
        const QHash<FamilyId, Family>& sourceFamilies,
        QHash<MinisteringDistrictId, MinisteringDistrict>& districts,
        QHash<MinisteringGroupId, MinisteringGroup>& groups,
        bool isAuthoritative);

    /// Find existing family by surname, display name, address, and phone.
    /// Matches by surname first, then narrows by display name if multiple,
    /// then by address, then by phone if still ambiguous.
    std::optional<Family> findMatchingFamily(
        const Family& pdfFamily,
        const QHash<FamilyId, Family>& families);

    /// Find matching person in family by first name and isParent.
    std::optional<Person> findMatchingPerson(
        const Person& person,
        const Family& family);

    /// Merge PDF family members into existing family.
    /// Builds personIdMapping. Returns updated family if any changes made.
    /// When isAuthoritative: can update names and add new members.
    /// When not authoritative: only fills empty fields.
    std::optional<Family> mergeFamilyMembers(
        const Family& sourceFamily,
        Family targetFamily,
        QHash<PersonId, PersonId>& personIdMapping,
        bool isAuthoritative);

    /// Determine if ministering PDF is authoritative for family data.
    /// Returns true if:
    /// - wardDirectoryDate is not set (first import), OR
    /// - PDF date is set AND is >= wardDirectoryDate
    bool isMinisteringAuthoritative(
        std::optional<QDate> pdfDate,
        std::optional<QDate> wardDirectoryDate) const;
};
