#pragma once

#include "UnitLookup.h"

#include <QStringList>

/// A unit lookup that records each request in order, as "ward <unit>" or "stake <unit>",
/// and answers when the test says.
class FakeUnitLookup : public UnitLookup
{
    Q_OBJECT

public:
    explicit FakeUnitLookup(QObject* parent);

    void lookupWard(const QString& wardUnitNumber) override;
    void lookupStake(const QString& stakeUnitNumber) override;

    /// Every lookup requested, in order.
    const QStringList& requests() const { return m_requests; }

    /// Answers the lookup of the ward with ward.
    void findWard(const QString& wardUnitNumber, const Ward& ward);

    /// Answers the lookup of the stake with stake.
    void findStake(const QString& stakeUnitNumber, const Stake& stake);

    /// Answers the lookup of the unit as failed.
    void fail(const QString& unitNumber);

private:
    QStringList m_requests;
};
