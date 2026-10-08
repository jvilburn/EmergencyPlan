#include "AccessControlTest.h"
#include "AccessRosterTest.h"
#include "AppSettings.h"
#include "AppSettingsTest.h"
#include "CryptoLinkTest.h"
#include "CryptoTypesTest.h"
#include "DocumentChainTest.h"
#include "DocumentCrypto.h"
#include "DocumentCryptoTest.h"
#include "DocumentFileTest.h"
#include "DocumentHeaderTest.h"
#include "DocumentLibraryTest.h"
#include "DocumentManagerTest.h"
#include "DocumentParseTest.h"
#include "DocumentSessionTest.h"
#include "FakeServer.h"
#include "FakeServerTest.h"
#include "FileOpenRouterTest.h"
#include "FormatVectorsTest.h"
#include "HttpServerApi.h"
#include "HttpServerApiTest.h"
#include "IdentityFileTest.h"
#include "IdentityServiceTest.h"
#include "ModelParsersTest.h"
#include "PairingTest.h"
#include "ReconcileTest.h"
#include "ResponseMergeTest.h"
#include "RosterChangeQueueTest.h"
#include "ServerConformanceTest.h"
#include "ServerConnectionTest.h"
#include "SmokeTest.h"
#include "WardDirectoryImportTest.h"

#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <functional>
#include <memory>

namespace
{

std::unique_ptr<ServerContract::ServerApi> makeFakeServer()
{
    return std::make_unique<FakeServer>();
}

std::unique_ptr<ServerContract::ServerApi> makeHttpServer(const AppSettings& settings)
{
    return std::make_unique<HttpServerApi>(settings, nullptr);
}

/// The real server's address for the conformance run, from the environment;
/// locally, the companion server under `wrangler dev` with its development clock
/// enabled. Unset, the run is skipped.
const char* const ServerAddressVariable = "WARD_PLANNING_SERVER_URL";

template<typename Suite, typename... Args>
int run(int argc, char** argv, Args&&... args)
{
    Suite suite(std::forward<Args>(args)...);
    return QTest::qExec(&suite, argc, argv);
}

} // namespace

// Each suite is qExec'd in turn. Passing argc/argv through means a command-line
// test-function filter applies to every suite, and a suite with no matching
// function returns non-zero. Run without arguments for the full pass.
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("Church");
    app.setApplicationName("Ward Planning Tests");

    if (!DocumentCrypto::initialise())
    {
        qCritical("libsodium failed to initialise");
        return 1;
    }

    // Keeps any QStandardPaths location a test touches out of the developer's
    // real profile.
    QStandardPaths::setTestModeEnabled(true);

    int status = 0;

    status |= run<SmokeTest>(argc, argv);
    status |= run<CryptoLinkTest>(argc, argv);
    status |= run<CryptoTypesTest>(argc, argv);
    status |= run<DocumentCryptoTest>(argc, argv);
    status |= run<DocumentHeaderTest>(argc, argv);
    status |= run<FormatVectorsTest>(argc, argv);
    status |= run<DocumentChainTest>(argc, argv);
    status |= run<AccessRosterTest>(argc, argv);
    status |= run<ModelParsersTest>(argc, argv);
    status |= run<DocumentParseTest>(argc, argv);
    status |= run<WardDirectoryImportTest>(argc, argv);
    status |= run<AccessControlTest>(argc, argv);
    status |= run<FakeServerTest>(argc, argv);
    status |= run<ServerConformanceTest>(argc, argv, makeFakeServer);

    if (qEnvironmentVariableIsSet(ServerAddressVariable))
    {
        // The run's settings live in a folder of their own, never the working one.
        QTemporaryDir folder;
        if (!folder.isValid())
        {
            qCritical("No temporary folder for the conformance run's settings");
            return 1;
        }
        std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
        settings->setServerAddressOverride(qEnvironmentVariable(ServerAddressVariable));
        status |= run<ServerConformanceTest>(argc, argv, std::bind(&makeHttpServer, std::ref(*settings)));
    }
    else
    {
        qInfo("%s is not set: the conformance run against a real server is skipped.", ServerAddressVariable);
    }
    status |= run<ReconcileTest>(argc, argv);
    status |= run<ResponseMergeTest>(argc, argv);
    status |= run<PairingTest>(argc, argv);
    status |= run<ServerConnectionTest>(argc, argv);
    status |= run<DocumentFileTest>(argc, argv);
    status |= run<DocumentSessionTest>(argc, argv);
    status |= run<RosterChangeQueueTest>(argc, argv);
    status |= run<AppSettingsTest>(argc, argv);
    status |= run<IdentityFileTest>(argc, argv);
    status |= run<IdentityServiceTest>(argc, argv);
    status |= run<DocumentLibraryTest>(argc, argv);
    status |= run<DocumentManagerTest>(argc, argv);
    status |= run<FileOpenRouterTest>(argc, argv);
    status |= run<HttpServerApiTest>(argc, argv);

    return status;
}
