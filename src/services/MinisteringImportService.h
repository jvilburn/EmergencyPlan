#pragma once

#include "Family.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"

#include <QObject>
#include <QString>
#include <QHash>

/// Result of ministering import operation
struct MinisteringImportResult
{
    bool success = false;
    bool isRSFormat = false;  // Auto-detected: true for RS, false for EQ

    QHash<QString, MinisteringDistrict> districts;
    QHash<QString, MinisteringGroup> groups;
    QHash<QString, Family> families;  // Final merged family list

    QStringList errors;
    QString wardName;
    QString wardUnitNumber;
    QString stakeName;
    QString stakeUnitNumber;
};

/// Service for importing ministering data from PDF files.
/// Handles ID resolution and merging with existing Document data.
class MinisteringImportService : public QObject
{
    Q_OBJECT

public:
    explicit MinisteringImportService(QObject* parent = nullptr);
    ~MinisteringImportService();

    /// Import ministering assignments from a PDF file (auto-detects EQ or RS).
    /// Matches parsed families to existing ones by surname.
    /// Updates member names from PDF (authoritative format).
    MinisteringImportResult importFromPdf(
        const QString& pdfPath,
        const QHash<QString, Family>& existingFamilies);

private:
    /// Merge source families into target, remapping IDs in groups and districts.
    /// - Matches families by surname, displayName, address, phone
    /// - Matches persons by firstName, isParent
    /// - isParent=true wins over isParent=false when merging
    /// - Unmatched source families are inserted into target
    void mergeFamilies(
        QHash<QString, Family>& targetFamilies,
        const QHash<QString, Family>& sourceFamilies,
        QHash<QString, MinisteringDistrict>& districts,
        QHash<QString, MinisteringGroup>& groups);

    /// Find existing family by surname, display name, address, and phone.
    /// Matches by surname first, then narrows by display name if multiple,
    /// then by address, then by phone if still ambiguous.
    std::optional<Family> findMatchingFamily(
        const Family& pdfFamily,
        const QHash<QString, Family>& families);

    /// Find matching person in family by first name and isParent.
    std::optional<Person> findMatchingPerson(
        const Person& person,
        const Family& family);

    /// Merge PDF family members into existing family.
    /// Builds personIdMapping. Returns updated family if any changes made.
    std::optional<Family> mergeFamilyMembers(
        const Family& sourceFamily,
        Family targetFamily,
        QHash<QString, QString>& personIdMapping);
};
