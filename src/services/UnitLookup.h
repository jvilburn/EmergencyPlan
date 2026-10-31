#pragma once

#include "Stake.h"
#include "Ward.h"

#include <QObject>
#include <QString>

/// Looks up a ward's or a stake's details by its unit number. Each lookup answers once,
/// with wardFound, stakeFound or lookupFailed.
class UnitLookup : public QObject
{
    Q_OBJECT

public:
    virtual void lookupWard(const QString& wardUnitNumber) = 0;
    virtual void lookupStake(const QString& stakeUnitNumber) = 0;

signals:
    /// The ward's chapel details and its stake unit number, as far as the lookup found them.
    void wardFound(const QString& wardUnitNumber, const Ward& ward);
    void stakeFound(const QString& stakeUnitNumber, const Stake& stake);
    void lookupFailed(const QString& unitNumber, const QString& error);

protected:
    explicit UnitLookup(QObject* parent);
};
