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
    void everyFieldEditDescribesEachField();
    void appliesWhereItsStartConditionHolds();
    void appliesAndChangesNothingWhereItsResultHolds();
    void conflictsAndTouchesNothingWhereAFieldMoved();
    void everyFieldMovedToAThirdValueConflicts_data();
    void everyFieldMovedToAThirdValueConflicts();
    void conflictsWhereTheFamilyIsGone();
    void conflictsWhereAChangedMemberIsGone();
    void editsToDifferentMembersBothApply();
    void memberRemovalConflictsAfterThatMemberChanged();
    void memberRemovalAppliesOntoAnUnchangedMemberAndItsInverseRestoresThem();
    void memberAdditionHoldsWhenThatPersonIsAlreadyThere();
    void addedMemberTakesItsIndexInTheSavedFamily();
    void inverseSwapsTheChangeUnderAnUndoHeading();
    void inverseOfTheEveryFieldEditRestoresTheDocumentExactly();
};
