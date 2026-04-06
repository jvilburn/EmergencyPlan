#include "DocumentMetadata.h"

#include <QJsonArray>

// ============================================================================
// Ward operations
// ============================================================================

void DocumentMetadata::addWard(const Ward& ward)
{
    m_wards.insert(ward.unitNumber(), ward);
}

void DocumentMetadata::updateWard(const Ward& ward)
{
    m_wards.insert(ward.unitNumber(), ward);
}

void DocumentMetadata::removeWard(const QString& unitNumber)
{
    m_wards.remove(unitNumber);
}

std::optional<Ward> DocumentMetadata::findWardByUnit(const QString& wardUnitNumber) const
{
    auto it = m_wards.find(wardUnitNumber);
    if (it != m_wards.end())
    {
        return *it;
    }
    return std::nullopt;
}

// ============================================================================
// Stake operations
// ============================================================================

void DocumentMetadata::addStake(const Stake& stake)
{
    m_stakes.insert(stake.unitNumber(), stake);
}

void DocumentMetadata::updateStake(const Stake& stake)
{
    m_stakes.insert(stake.unitNumber(), stake);
}

void DocumentMetadata::removeStake(const QString& unitNumber)
{
    m_stakes.remove(unitNumber);
}

std::optional<Stake> DocumentMetadata::findStakeByUnit(const QString& unitNumber) const
{
    auto it = m_stakes.find(unitNumber);
    if (it != m_stakes.end())
    {
        return *it;
    }
    return std::nullopt;
}

std::optional<Stake> DocumentMetadata::findStake(const QString& wardUnitNumber) const
{
    for (const Stake& stake : m_stakes)
    {
        if (stake.wardUnitNumbers().contains(wardUnitNumber))
        {
            return stake;
        }
    }
    return std::nullopt;
}

// ============================================================================
// Combined operations
// ============================================================================

QString DocumentMetadata::addOrUpdateWard(const QString& wardUnitNumber, const QString& wardName,
                                          const QString& stakeUnitNumber, const QString& stakeName)
{
    // Create or update the ward
    Ward ward;
    if (auto existing = findWardByUnit(wardUnitNumber))
    {
        ward = *existing;
    }
    ward.setUnitNumber(wardUnitNumber);
    if (!wardName.isEmpty())
    {
        ward.setName(wardName);
    }
    m_wards.insert(wardUnitNumber, ward);

    // If stake info provided, create or update stake
    if (!stakeUnitNumber.isEmpty())
    {
        Stake stake;
        if (auto existing = findStakeByUnit(stakeUnitNumber))
        {
            stake = *existing;
        }
        stake.setUnitNumber(stakeUnitNumber);
        if (!stakeName.isEmpty())
        {
            stake.setName(stakeName);
        }
        // Add ward to stake's ward list
        QSet<QString> wardUnits = stake.wardUnitNumbers();
        wardUnits.insert(wardUnitNumber);
        stake.setWardUnitNumbers(wardUnits);
        m_stakes.insert(stakeUnitNumber, stake);
        return stakeUnitNumber;
    }

    return QString();
}

void DocumentMetadata::removeWardByUnit(const QString& wardUnitNumber)
{
    m_wards.remove(wardUnitNumber);
    // Note: We don't remove the ward from stake's wardUnitNumbers list
    // Stakes are reference data that can be reused
}

QString DocumentMetadata::suggestedFilename() const
{
    int wardCount = m_wards.size();
    if (wardCount == 0)
    {
        return QString();
    }

    // 1 ward
    if (wardCount == 1)
    {
        QString name = m_wards.begin()->name();
        return name.isEmpty() ? QString() : name + " Ward";
    }

    // 2+ wards: check stake membership
    QSet<QString> stakeUnits;
    bool allStakesKnown = true;

    for (const Ward& ward : m_wards)
    {
        std::optional<Stake> stake = findStake(ward.unitNumber());
        if (stake.has_value())
        {
            stakeUnits.insert(stake->unitNumber());
        }
        else
        {
            allStakesKnown = false;
        }
    }

    // All wards in same known stake
    if (allStakesKnown && stakeUnits.size() == 1)
    {
        std::optional<Stake> stake = findStakeByUnit(*stakeUnits.begin());
        if (stake.has_value() && !stake->name().isEmpty())
        {
            return stake->name() + " Stake";
        }
    }

    // 2 wards, stake unknown for either
    if (wardCount == 2)
    {
        auto it = m_wards.begin();
        QString first = it->name();
        ++it;
        QString second = it->name();
        if (!first.isEmpty() && !second.isEmpty())
        {
            return QString("%1 And %2 Wards").arg(first, second);
        }
    }

    // 3+ wards with unknown stake
    if (!allStakesKnown)
    {
        QString first = m_wards.begin()->name();
        if (!first.isEmpty())
        {
            return QString("%1 Multi-Ward").arg(first);
        }
    }

    // 3+ wards, different known stakes
    if (!stakeUnits.isEmpty())
    {
        std::optional<Stake> stake = findStakeByUnit(*stakeUnits.begin());
        if (stake.has_value() && !stake->name().isEmpty())
        {
            return QString("%1 Multi-Stake").arg(stake->name());
        }
    }

    return QString();
}

// ============================================================================
// JSON serialization
// ============================================================================

void DocumentMetadata::writeToJson(QJsonObject& json) const
{
    if (!m_stakes.isEmpty())
    {
        QJsonArray array;
        for (const Stake& stake : m_stakes)
        {
            array.append(stake.toJson());
        }
        json["stakes"] = array;
    }

    if (!m_wards.isEmpty())
    {
        QJsonArray array;
        for (const Ward& ward : m_wards)
        {
            array.append(ward.toJson());
        }
        json["wards"] = array;
    }
}

void DocumentMetadata::readFromJson(const QJsonObject& json)
{
    if (json.contains("stakes"))
    {
        QJsonArray array = json["stakes"].toArray();
        for (const QJsonValue& value : array)
        {
            Stake stake = Stake::fromJson(value.toObject());
            m_stakes.insert(stake.unitNumber(), stake);
        }
    }

    if (json.contains("wards"))
    {
        QJsonArray array = json["wards"].toArray();
        for (const QJsonValue& value : array)
        {
            Ward ward = Ward::fromJson(value.toObject());
            m_wards.insert(ward.unitNumber(), ward);
        }
    }
}

// ============================================================================
// Equality
// ============================================================================

bool DocumentMetadata::operator==(const DocumentMetadata& other) const
{
    return m_wards == other.m_wards && m_stakes == other.m_stakes;
}
