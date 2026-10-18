#pragma once

#include "Ward.h"
#include "Stake.h"

#include <QHash>
#include <QString>
#include <QJsonObject>
#include <optional>

/// Manages ward/stake hierarchy and document naming.
/// Extracted from Document to keep Document focused on core data collections.
class DocumentMetadata
{
public:
    DocumentMetadata() = default;

    // ========================================================================
    // Getters
    // ========================================================================
    const QHash<QString, Ward>& wards() const { return m_wards; }
    const QHash<QString, Stake>& stakes() const { return m_stakes; }

    // ========================================================================
    // Ward operations
    // ========================================================================
    void addWard(const Ward& ward);
    void updateWard(const Ward& ward);
    std::optional<Ward> findWardByUnit(const QString& wardUnitNumber) const;

    // ========================================================================
    // Stake operations
    // ========================================================================
    void addStake(const Stake& stake);
    void updateStake(const Stake& stake);
    void removeStake(const QString& unitNumber);
    std::optional<Stake> findStakeByUnit(const QString& unitNumber) const;

    // ========================================================================
    // Combined operations
    // ========================================================================

    /// Creates/updates Ward and optionally its Stake.
    /// Returns the stake unit number where ward was placed.
    QString addOrUpdateWard(const QString& wardUnitNumber, const QString& wardName,
                            const QString& stakeUnitNumber,
                            const QString& stakeName);

    /// Removes a ward by unit number (for undo).
    void removeWardByUnit(const QString& wardUnitNumber);

    /// The ward's name as people see it, from the names of its wards.
    QString wardName() const;

    // ========================================================================
    // JSON serialization
    // ========================================================================
    void writeToJson(QJsonObject& json) const;
    void readFromJson(const QJsonObject& json);

    // ========================================================================
    // Equality
    // ========================================================================
    bool operator==(const DocumentMetadata& other) const;
    bool operator!=(const DocumentMetadata& other) const { return !(*this == other); }

private:
    QHash<QString, Ward> m_wards;    // keyed by ward unit number
    QHash<QString, Stake> m_stakes;  // keyed by stake unit number
};
