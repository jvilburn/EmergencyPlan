#pragma once

#include <QObject>

/// Section 6.3: signing in and signing requests.
class ServerConnectionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void theFirstCallSignsInAndSucceeds();
    void anExpiredSessionIsSignedIntoAgain();
    void aRefusedSignInGivesOneUnauthenticatedReplyAndNoRetry();
    void callsMadeBeforeTheFirstSignInShareOneChallenge();
    void signingInRegistersThePerson();
    void aDeviceWithNoPersonSignsInToLink();
    void anUnlinkedDeviceHearsItFromEveryCallWithoutSigningInAgain();
    void aReplyWhoseContextWasDestroyedIsDropped();
};
