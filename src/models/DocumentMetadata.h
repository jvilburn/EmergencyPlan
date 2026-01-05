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
    void removeWard(const QString& unitNumber);
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
                            const QString& stakeUnitNumber = QString(),
                            const QString& stakeName = QString());

    /// Removes a ward by unit number (for undo).
    void removeWardByUnit(const QString& wardUnitNumber);

    /// Suggested filename based on wards/stakes in document.
    QString suggestedFilename() const;

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
    /// Find stake containing a ward by ward unit number.
    std::optional<Stake> findStake(const QString& wardUnitNumber) const;

    QHash<QString, Ward> m_wards;    // keyed by ward unit number
    QHash<QString, Stake> m_stakes;  // keyed by stake unit number
};
