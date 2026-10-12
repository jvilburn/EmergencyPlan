#pragma once

#include <QObject>

/// What the access notice says changed between two rosters, one line per change.
class RosterDiffTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void theSameRosterSaysNothing();
    void aJoinSaysWhoAsWhat();
    void aRemovalSaysWhoNoLongerHasAccess();
    void aRoleChangeSaysTheNewRole();
    void aDeviceAddedOrUnlinkedNamesBoth();
    void anInviteMadeNamesItsMakerAndLabel();
    void aWithdrawnInviteIsNotNews();
    void severalChangesComeInOrder();
};
