#pragma once

#include <QObject>

class UpdateFamilyCommandTest : public QObject
{
    Q_OBJECT

public:
    explicit UpdateFamilyCommandTest(QObject* parent);

private slots:
    void editHoldsOnlyTheFieldsThatDiffer();
    void editThatChangesNothingIsNoCommand();
    void descriptionIsAHeadingAndALinePerValueSet();
    void appliesWhereItsStartConditionHolds();
    void appliesAndChangesNothingWhereItsResultHolds();
    void conflictsAndTouchesNothingWhereAFieldMoved();
    void conflictsWhereTheFamilyIsGone();
    void conflictsWhereAChangedMemberIsGone();
    void editsToDifferentMembersBothApply();
    void memberRemovalConflictsAfterThatMemberChanged();
    void memberRemovalAppliesOntoAnUnchangedMemberAndItsInverseRestoresThem();
    void memberAdditionHoldsWhenThatPersonIsAlreadyThere();
    void inverseSwapsTheChangeUnderAnUndoHeading();
};
