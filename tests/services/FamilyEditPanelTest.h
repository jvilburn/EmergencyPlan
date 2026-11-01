#pragma once

#include <QObject>

/// The family edit panel places an added member by the member order rule, again as its
/// details are filled in, and saves the family in that order. Its coordinate fields show the
/// entry for the address it shows, and hand back only the coordinates the user typed. Look Up
/// stays disabled while its request is pending. Its note field shows the family's note and
/// hands back what the user typed, trimmed.
class FamilyEditPanelTest : public QObject
{
    Q_OBJECT

public:
    explicit FamilyEditPanelTest(QObject* parent);

private slots:
    void addedChildMovesIntoPlaceWhenItsBirthYearIsTyped();
    void addedChildStaysLastWhileAMemberHasNoBirthYear();
    void coordinateFieldsAreEnabledOnlyWhileTheAddressIsNotEmpty();
    void coordinatesShownFromTheEntryAreNotTyped();
    void typedCoordinatesAreReturnedAndMakeThePanelDirty();
    void saveIsDisabledWhileTypedCoordinatesDoNotParse();
    void refreshShowsTheNewEntryUnlessCoordinatesAreTyped();
    void lookUpIsDisabledWhileItsRequestIsPending();
    void typedFamilyNoteIsReturnedTrimmedAndMakesThePanelDirty();
};
