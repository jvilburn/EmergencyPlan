#include "DocumentMetadata.h"

#include <QJsonArray>
#include <QObject>
#include <QStringList>

#include <algorithm>

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

QString DocumentMetadata::wardName() const
{
    QStringList names;
    for (const Ward& ward : m_wards)
    {
        names.append(ward.name());
    }
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) { return QString::localeAwareCompare(a, b) < 0; });
    if (names.isEmpty())
    {
        return QObject::tr("Untitled ward");
    }
    if (names.size() == 1)
    {
        return QObject::tr("%1 Ward").arg(names.first());
    }
    QString last = names.takeLast();
    return QObject::tr("%1 and %2 Wards").arg(names.join(QObject::tr(", ")), last);
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
