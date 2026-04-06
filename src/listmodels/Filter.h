#pragma once

#include <QObject>
#include <QList>
#include <QSet>
#include <QString>
#include <optional>

#include "EmergencyResponse.h"
#include "Gender.h"
#include "Id.h"
#include "ResponseArea.h"

class Document;
class Family;
class Person;

/// Age filter mode for families and persons.
enum class AgeFilter
{
    All,        ///< No age filtering
    Adults,     ///< Adults only (persons without known child age count as adults)
    Children    ///< Children only (persons with isChild() true)
};

/// Mapped filter mode for families.
enum class MappedFilter
{
    All,        ///< No filtering by map status
    Mapped,     ///< Only families with coordinates
    Unmapped    ///< Only families without coordinates
};

/// Filter state and logic for families and persons.
/// Each view owns its own Filter instance.
class Filter : public QObject
{
    Q_OBJECT

public:
    explicit Filter(QObject* parent);

    // Getters
    QString searchText() const { return m_searchText; }
    QSet<TagId> tagIds() const { return m_tagIds; }
    QSet<TeamId> teamIds() const { return m_teamIds; }
    QSet<EmergencyAssetId> assetTypeIds() const { return m_assetTypeIds; }
    QSet<QString> callings() const { return m_callings; }
    std::optional<Gender> gender() const { return m_gender; }
    AgeFilter ageFilter() const { return m_ageFilter; }
    std::optional<int> specificAge() const { return m_specificAge; }
    MappedFilter mappedFilter() const { return m_mappedFilter; }
    bool onlyWithContact() const { return m_onlyWithContact; }
    QSet<QString> specialNeeds() const { return m_specialNeeds; }
    bool hasAnySpecialNeed() const { return m_hasAnySpecialNeed; }
    QSet<ResponseArea> responseAreas() const { return m_responseAreas; }
    std::optional<EffectiveContactStatus> contactStatusFilter() const { return m_contactStatusFilter; }

    // Setters (emit changed() signal)
    void setSearchText(const QString& text);
    void setTagIds(const QSet<TagId>& ids);
    void setTeamIds(const QSet<TeamId>& ids);
    void setAssetTypeIds(const QSet<EmergencyAssetId>& ids);
    void setCallings(const QSet<QString>& callings);
    void setGender(std::optional<Gender> gender);
    void setAgeFilter(AgeFilter filter);
    void setSpecificAge(std::optional<int> age);
    void setMappedFilter(MappedFilter filter);
    void setOnlyWithContact(bool value);
    void setSpecialNeeds(const QSet<QString>& needs);
    void addSpecialNeed(const QString& need);
    void removeSpecialNeed(const QString& need);
    void setHasAnySpecialNeed(bool value);
    void setResponseAreas(const QSet<ResponseArea>& areas);
    void addResponseArea(ResponseArea area);
    void removeResponseArea(ResponseArea area);
    void setContactStatusFilter(std::optional<EffectiveContactStatus> status);

    // Bulk operations
    void clear();
    bool isEmpty() const;

    // Check if individual items pass current filter criteria
    bool passes(const Document& document, const Family& family) const;
    bool passes(const Document& document, const Person& person) const;

signals:
    void changed();

private:
    /// Family-only criteria: mapped, contact (family.hasContact()).
    bool passesFamilyCriteria(const Family& family) const;

    /// Person-only criteria: callings, gender, age.
    bool passesPersonCriteria(const Person& person) const;

    // Search helpers
    static bool personContainsWord(const Person& person, const QString& word);
    static bool familyContainsWord(const Family& family, const QString& word);

    // Tag/asset helpers (separate family-level and person-level)
    bool hasFamilyLevelTag(const Document& document, const FamilyId& familyId) const;
    bool hasPersonLevelTag(const Document& document, const PersonId& personId) const;
    bool isOnTeam(const Document& document, const PersonId& personId) const;
    bool hasResponseArea(const Document& document, const PersonId& personId) const;

    QString m_searchText;
    QSet<TagId> m_tagIds;
    QSet<TeamId> m_teamIds;
    QSet<EmergencyAssetId> m_assetTypeIds;
    QSet<QString> m_callings;
    std::optional<Gender> m_gender;
    AgeFilter m_ageFilter = AgeFilter::All;
    std::optional<int> m_specificAge;
    MappedFilter m_mappedFilter = MappedFilter::All;
    bool m_onlyWithContact = false;
    QSet<QString> m_specialNeeds;
    bool m_hasAnySpecialNeed = false;
    QSet<ResponseArea> m_responseAreas;
    std::optional<EffectiveContactStatus> m_contactStatusFilter;
};
