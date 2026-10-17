#pragma once

#include <QObject>

class FieldChangeTest : public QObject
{
    Q_OBJECT

public:
    explicit FieldChangeTest(QObject* parent);

private slots:
    void changeHoldsAtBeforeOrAfterOnly();
    void changeSwappedExchangesItsEnds();
    void changeBetweenEqualValuesIsNone();
    void setChangeBetweenHoldsTheIdsAddedAndRemoved();
    void setChangeAdditionPresentAndRemovalAbsentAreDone();
    void setChangesAddingDifferentIdsBothLand();
    void setChangeSwappedTakesItselfBack();
};
