#pragma once

#include <QObject>

/// The controls FakeServer offers that the real server must never have.
///
/// These exist so the client's negative tests can make the server misbehave;
/// this suite shows only that each control does what it says. The rules
/// themselves are in ServerConformanceTest.
class FakeServerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void offlineRefusesEverything();
    void aForcedConflictRefusesTheNextUploadOnly();
    void aLeaseCanBeHeldByAnotherDevice();
    void limitsAreConfigurable();

    // Replies
    void aReplyNeverArrivesBeforeTheCallReturns();
    void heldRepliesArriveInCallOrderOneAtATime();
    void anUploadTakesEffectWhileItsReplyIsHeld();
    void aLostReplysUploadIsStillTheCurrentVersion();
};
