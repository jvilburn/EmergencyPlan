#pragma once

#include "ServerContract.h"

#include <QObject>

#include <functional>
#include <memory>

/// The rules every Ward Planning server must enforce, written against ServerApi
/// alone.
///
/// Runs against FakeServer in the ordinary suite, and against the real server
/// over HTTP in the conformance run. It therefore uses no fake-only control, makes
/// a fresh server per test where it can, uses fresh random ids and keys everywhere
/// so it can share a real server with other runs, and signs in again after every
/// clock advance rather than assuming how long a session lasts.
///
/// A test that passes here and fails against the real server is a disagreement
/// to resolve in one of them, never a test to loosen.
class ServerConformanceTest : public QObject
{
    Q_OBJECT

public:
    explicit ServerConformanceTest(std::function<std::unique_ptr<ServerContract::ServerApi>()> makeServer);

private slots:
    void initTestCase();
    void init();
    void cleanup();

    // Signing in
    void authenticatesASignedChallenge();
    void refusesAChallengeSignedByAnotherKey();
    void aChallengeIsGoodForOneLogin();
    void anExpiredSessionIsUnauthenticated();

    // People and their devices
    void registersANewPersonWithThisDevice();
    void registeringAgainChangesNothing();
    void aDeviceCannotClaimAnotherPerson();
    void anApprovedLinkAddsTheDeviceToThePerson();
    void anApprovalStoresEveryResealingOrNone();
    void anApprovalWithNoResealingsAddsTheDevice();
    void aLinkCodeExpires();
    void aDeviceWithAPersonCannotStartALink();
    void unlinkingCutsTheDeviceOffEveryWard();
    void aPersonsLastDeviceCannotBeUnlinked();
    void anUnlinkedDeviceMayStartOverAsANewPerson();

    // Documents and versions
    void registersADocumentWithItsCreatorAsAdmin();
    void aNewWardIsSealedOnlyToItsCreatorsDevices();
    void refusesATakenDocumentId();
    void writesAreConditionalOnTheStartingVersion();
    void anUploadListsOnlyAllowListedDevices();
    void aDownloadIsOneConsistentSnapshot();

    // Who has access
    void onlyMembersReadOrWrite();
    void aPersonJoinsOnlyThroughAGrantedRequest();
    void aGrantStoresTheResealedVersionWithTheMembership();
    void aGrantOnAStaleVersionChangesNothing();
    void aGrantSealingToAStrangerIsRefused();
    void everyDeviceOfAMemberIsAllowListed();
    void onlyAnAdminGrantsRemovesOrChangesRoles();
    void aDeclinedRequestSaysSo();
    void aRemovalCutsThePersonOffAtOnce();
    void leavingCutsThePersonOffAtOnce();
    void theLastAdminCannotLeaveOrStepDown();
    void theLastPersonLeavingEndsTheWard();
    void aRemovedPersonMayBeGrantedAgain();
    void theHistoryListsEachChangeOnce();

    // The lease
    void theLeaseIsHeldByOneDeviceAtATime();
    void aLeaseExpires();

private:
    ServerContract::ServerApi& server() { return *m_server; }

    std::function<std::unique_ptr<ServerContract::ServerApi>()> m_makeServer;
    std::unique_ptr<ServerContract::ServerApi> m_server;
};
