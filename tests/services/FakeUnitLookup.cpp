#include "FakeUnitLookup.h"

FakeUnitLookup::FakeUnitLookup(QObject* parent)
    : UnitLookup(parent)
{
}

void FakeUnitLookup::lookupWard(const QString& wardUnitNumber)
{
    m_requests.append(QString("ward %1").arg(wardUnitNumber));
}

void FakeUnitLookup::lookupStake(const QString& stakeUnitNumber)
{
    m_requests.append(QString("stake %1").arg(stakeUnitNumber));
}

void FakeUnitLookup::findWard(const QString& wardUnitNumber, const Ward& ward)
{
    emit wardFound(wardUnitNumber, ward);
}

void FakeUnitLookup::findStake(const QString& stakeUnitNumber, const Stake& stake)
{
    emit stakeFound(stakeUnitNumber, stake);
}

void FakeUnitLookup::fail(const QString& unitNumber)
{
    emit lookupFailed(unitNumber, "no such unit");
}
