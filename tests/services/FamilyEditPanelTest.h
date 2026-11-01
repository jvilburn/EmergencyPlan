#pragma once

#include <QObject>

/// The family edit panel places an added member by the member order rule, again as its
/// details are filled in, and saves the family in that order. Its coordinate fields show the
/// entry for the address it shows, and hand back only the coordinates the user typed.
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
};
