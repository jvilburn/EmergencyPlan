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
/// a fresh server per test, and uses fresh random ids and keys everywhere so it can
/// share a real server with other runs.
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

    // Documents and versions
    void registersVersionOneByItsMaker();
    void aRegistrationMustNameItsSigner();
    void aRegistrationWithABadSignatureIsAnInvalidRequest();
    void refusesATakenDocumentId();
    void aMalformedIdIsAnInvalidRequest();
    void readsNeedNoSigner();
    void anUploadByAKeyTheHeaderNamesLands();
    void anUploadByAKeyTheHeaderDoesNotNameIsRefused();
    void anUploadWhoseHeaderNamesAnotherDocumentIsRefused();
    void aKeyTheCurrentHeaderDropsCanNoLongerWrite();
    void aBadSignatureIsAnInvalidRequest();
    void writesAreConditionalOnTheStartingVersion();
    void aDownloadIsOneConsistentSnapshot();

    // The lease
    void theLeaseIsHeldByOneDeviceAtATime();
    void onlyAKeyHolderTakesOrReleasesTheLease();
    void aLeaseExpires();

private:
    ServerContract::ServerApi& server() { return *m_server; }

    std::function<std::unique_ptr<ServerContract::ServerApi>()> m_makeServer;
    std::unique_ptr<ServerContract::ServerApi> m_server;
};
