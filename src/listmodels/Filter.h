#pragma once

#include <QObject>
#include <QList>
#include <QSet>
#include <QString>
#include <optional>

#include "Gender.h"

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
    explicit Filter(QObject* parent = nullptr);

    // Getters
    QString searchText() const { return m_searchText; }
    QSet<QString> tagIds() const { return m_tagIds; }
    QSet<QString> teamIds() const { return m_teamIds; }
    QSet<QString> resourceTypeIds() const { return m_resourceTypeIds; }
    QSet<QString> callings() const { return m_callings; }
    std::optional<Gender> gender() const { return m_gender; }
    AgeFilter ageFilter() const { return m_ageFilter; }
    std::optional<int> specificAge() const { return m_specificAge; }
    MappedFilter mappedFilter() const { return m_mappedFilter; }
    bool onlyWithContact() const { return m_onlyWithContact; }
    QSet<QString> specialNeeds() const { return m_specialNeeds; }
    bool hasAnySpecialNeed() const { return m_hasAnySpecialNeed; }

    // Setters (emit changed() signal)
    void setSearchText(const QString& text);
    void setTagIds(const QSet<QString>& ids);
    void setTeamIds(const QSet<QString>& ids);
    void setResourceTypeIds(const QSet<QString>& ids);
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

    // Tag/resource helpers (separate family-level and person-level)
    bool hasFamilyLevelTag(const Document& document, const QString& familyId) const;
    bool hasPersonLevelTag(const Document& document, const QString& personId) const;
    bool hasFamilyLevelResource(const Document& document, const QString& familyId) const;
    bool hasPersonLevelResource(const Document& document, const QString& personId) const;
    bool isOnTeam(const Document& document, const QString& personId) const;

    QString m_searchText;
    QSet<QString> m_tagIds;
    QSet<QString> m_teamIds;
    QSet<QString> m_resourceTypeIds;
    QSet<QString> m_callings;
    std::optional<Gender> m_gender;
    AgeFilter m_ageFilter = AgeFilter::All;
    std::optional<int> m_specificAge;
    MappedFilter m_mappedFilter = MappedFilter::All;
    bool m_onlyWithContact = false;
    QSet<QString> m_specialNeeds;
    bool m_hasAnySpecialNeed = false;
};
