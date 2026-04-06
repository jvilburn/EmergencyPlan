#pragma once

#include <QString>
#include <QSet>
#include <QDate>
#include <QJsonObject>
#include <QJsonArray>
#include <optional>

#include "Id.h"

/// Unified ministering group for both EQ and RS.
/// EQ groups: minister families (familyIds)
/// RS groups: minister persons (ministeredPersonIds)
class MinisteringGroup
{
public:
    MinisteringGroup() = default;

    /// Factory method for EQ groups (ministers to families)
    static MinisteringGroup createEQ(
        const QSet<PersonId>& ministerIds,
        const QSet<FamilyId>& familyIds);

    /// Factory method for RS groups (ministers to persons)
    static MinisteringGroup createRS(
        const QSet<PersonId>& ministerIds,
        const QSet<PersonId>& ministeredPersonIds);

    // Getters
    const MinisteringGroupId& id() const { return m_id; }
    const QSet<PersonId>& ministerIds() const { return m_ministerIds; }
    const QSet<FamilyId>& familyIds() const { return m_familyIds; }
    const QSet<PersonId>& ministeredPersonIds() const { return m_ministeredPersonIds; }
    std::optional<QDate> interviewedDate() const { return m_interviewedDate; }
    std::optional<PersonId> presidencyMemberId() const { return m_presidencyMemberId; }

    // Setters
    void setMinisterIds(const QSet<PersonId>& ministerIds) { m_ministerIds = ministerIds; }
    void setFamilyIds(const QSet<FamilyId>& familyIds) { m_familyIds = familyIds; }
    void setMinisteredPersonIds(const QSet<PersonId>& ids) { m_ministeredPersonIds = ids; }
    void setInterviewedDate(std::optional<QDate> date) { m_interviewedDate = date; }
    void setPresidencyMemberId(std::optional<PersonId> id) { m_presidencyMemberId = id; }
    void setIsRSGroup(bool isRS) { m_isRSFormat = isRS; }
    void addMinister(const PersonId& personId) { m_ministerIds.insert(personId); }
    void removeMinister(const PersonId& personId) { m_ministerIds.remove(personId); }
    void addFamily(const FamilyId& familyId) { m_familyIds.insert(familyId); }
    void removeFamily(const FamilyId& familyId) { m_familyIds.remove(familyId); }
    void addMinisteredPerson(const PersonId& personId) { m_ministeredPersonIds.insert(personId); }
    void removeMinisteredPerson(const PersonId& personId) { m_ministeredPersonIds.remove(personId); }

    // Computed properties
    bool hasMinisters() const { return !m_ministerIds.isEmpty(); }
    int ministerCount() const { return m_ministerIds.size(); }
    bool hasFamilies() const { return !m_familyIds.isEmpty(); }
    int familyCount() const { return m_familyIds.size(); }
    bool hasMinisteredPersons() const { return !m_ministeredPersonIds.isEmpty(); }
    int ministeredPersonCount() const { return m_ministeredPersonIds.size(); }

    /// Returns true if this is an RS group (ministers to persons, not families)
    bool isRSGroup() const { return m_isRSFormat; }

    // JSON serialization
    QJsonObject toJson() const;
    static MinisteringGroup fromJson(const QJsonObject& json);

    // Value-based equality
    bool operator==(const MinisteringGroup& other) const;
    bool operator!=(const MinisteringGroup& other) const { return !(*this == other); }

private:
    MinisteringGroupId m_id;
    bool m_isRSFormat = false;
    QSet<PersonId> m_ministerIds;
    QSet<FamilyId> m_familyIds;             // EQ: populated
    QSet<PersonId> m_ministeredPersonIds;   // RS: populated
    std::optional<QDate> m_interviewedDate;
    std::optional<PersonId> m_presidencyMemberId;
};
