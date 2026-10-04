#pragma once

#include "ServerContract.h"

#include <QObject>

#include <functional>
#include <memory>

/// The rules every Ward Planning server must enforce, written against ServerApi
/// alone.
///
/// Runs against FakeServer in the ordinary suite, and against the real server
/// over HTTP in the conformance run. It therefore uses no fake-only
/// control, makes a fresh server per test where it can, uses fresh random ids
/// and keys everywhere so it can share a real server with other runs, and signs
/// in again after every clock advance rather than assuming how long a session
/// lasts.
///
/// A test that passes here and fails against the real server is a disagreement
/// to resolve in one of them, never a test to loosen.
class ServerConformanceTest : public QObject
{
    Q_OBJECT

public:
    explicit ServerConformanceTest(
        std::function<std::unique_ptr<ServerContract::ServerApi>()> makeServer);

private slots:
    void initTestCase();
    void init();
    void cleanup();

    // Authentication
    void authenticatesASignedChallenge();
    void refusesAChallengeSignedUnderAnotherTagOrKey();
    void aChallengeIsGoodForOneLogin();
    void anExpiredSessionIsUnauthenticated();
    void aMutatingRequestMustBeSignedByTheCallingDevice();
    void aRequestSignatureCoversItsBody();
    void writesOpenToNonMembersAreSignedToo();

    // Registering a document
    void registersADocumentWithOnePerson();
    void aFirstHeaderListsExactlyOnePerson();
    void aFirstHeaderBringsOnlyDevicesPairedWithTheRegisteringDevice();
    void aFirstHeaderIsWrittenAndSignedByTheRegisteringDevice();
    void refusesATakenDocumentId();
    void registrationIsRateLimited();

    // Versions
    void writesAreConditionalOnTheStartingVersion();
    void versionAndKeyGenerationNeverDecrease();
    void oneUploadRaisesTheSaveCounterOnlySoFar();
    void uploadsAreRateLimited();
    void thePayloadHashMustMatchTheBlob();
    void theUploaderIsTheWriter();

    // The allow-list
    void onlyCurrentDevicesReadOrWrite();
    void anAcceptedUploadChangesTheAllowListAtOnce();
    void aDownloadIsOneConsistentSnapshot();
    void aSnapshotEndsWithTheHeaderOfItsVersion();

    // The chain
    void everyUploadMustExtendTheChain();
    void keepsOnlyHeadersThatChangedMembership();
    void aFormerDeviceReadsTheChainOnlyThroughItsRemoval();
    void aRemovedDeviceReadsItsRemovalWithoutTheHead();
    void aDeviceNeverListedCannotReadTheChain();

    // Members and devices joining
    void aDeviceJoinsOnlyByRequest();
    void aDeviceRequestsOnlyForItself();
    void aCompletedPairingLetsTheResponderLinkTheInitiator();
    void aPairingPermitsOnlyThePersonOfTheResponder();
    void aPairingPermitHoldsInEveryWardOfThatPerson();
    void anUnfinishedPairingPermitsNothing();

    // Removals and leaving
    void anAdminRemovesAPersonAtOnce();
    void aMemberCannotRemoveAnyone();
    void aMemberLeavesByWritingTheirOwnLeave();
    void aRemovedPersonCanProveARemovalByAnAdminPromotedSince();
    void aLastPersonsLeaveEndsTheDocument();
    void aPersonWhoLeftOrWasRemovedIsGrantedAgainUnderTheSameMemberId();

    // Access requests
    void requestStatusIsReadableByItsDevice();
    void accessRequestsAreRateLimited();
    void aDeviceHasOneRequestPerDocument();
    void aRequestCannotClaimAnotherDevicesSealingKey();
    void accessRequestsCarryTheirPairingCodeToMembers();

    // Authentication levels
    void deviceAuthenticationReachesOnlyItsEndpoints();
    void myDocumentsListsOnlyDocumentsTheDeviceIsIn();

    // Pairings
    void pairingStepsAreWrittenOnceAndInOrder();
    void pairingKeysMustBeTheCallersOwn();
    void aPairingIsReadableOnlyByItsParties();
    void aPairingExpires();

    // Commits
    void aRefusedUploadChangesNothing();

    // The lease
    void theLeaseIsHeldByOneDeviceAtATime();
    void aLeaseExpires();

private:
    ServerContract::ServerApi& server() { return *m_server; }

    std::function<std::unique_ptr<ServerContract::ServerApi>()> m_makeServer;
    std::unique_ptr<ServerContract::ServerApi> m_server;
};
