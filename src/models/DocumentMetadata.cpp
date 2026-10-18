#include "DocumentMetadata.h"

#include <QJsonArray>
#include <QObject>
#include <QStringList>

#include <algorithm>

// ============================================================================
// Ward operations
// ============================================================================

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

void DocumentMetadata::updateStake(const Stake& stake)
{
    m_stakes.insert(stake.unitNumber(), stake);
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
// Ward name
// ============================================================================

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
