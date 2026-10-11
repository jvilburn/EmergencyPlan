#pragma once

#include <QObject>

/// Changes to who has access, read from headers alone.
class AccessEventsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void eachKindComesFromTheHeaderPairThatMakesIt();
    void aLeaveIsLeftNotRemoved();
    void steppingDownIsToldApartFromBeingMadeAMember();
    void aGrantThenARemovalBetweenTwoSyncsYieldsBoth();
    void linkingOrUnlinkingOnesOwnDeviceYieldsNothing();
    void namesComeFromTheCurrentRosterThenTheBaseThenAFormerMember();
    void eachDeviceIsAddedByTheWriterOfTheHeaderThatFirstListsIt();
};
