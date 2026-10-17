#pragma once

#include <QObject>

class ChangeTest : public QObject
{
    Q_OBJECT

public:
    explicit ChangeTest(QObject* parent);

private slots:
    void changeHoldsAtBeforeOrAfterOnly();
    void changeSwappedExchangesItsEnds();
    void changeBetweenEqualValuesIsNone();
    void setChangeBetweenHoldsTheIdsAddedAndRemoved();
    void setChangeAdditionPresentAndRemovalAbsentAreDone();
    void setChangesAddingDifferentIdsBothLand();
    void setChangeSwappedTakesItselfBack();
};
