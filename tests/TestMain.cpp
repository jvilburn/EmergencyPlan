#include "AccessControlTest.h"
#include "AccessRosterTest.h"
#include "CryptoLinkTest.h"
#include "CryptoTypesTest.h"
#include "DocumentChainTest.h"
#include "DocumentCrypto.h"
#include "DocumentCryptoTest.h"
#include "DocumentFileTest.h"
#include "DocumentHeaderTest.h"
#include "DocumentParseTest.h"
#include "DocumentSessionTest.h"
#include "FakeServer.h"
#include "FakeServerTest.h"
#include "FormatVectorsTest.h"
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
#include <QtTest>

#include <memory>

namespace
{

std::unique_ptr<ServerContract::ServerApi> makeFakeServer()
{
    return std::make_unique<FakeServer>();
}

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

    // Keeps every settings write and every QStandardPaths location out of the
    // developer's real profile, so no test depends on state left by a previous run.
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
    status |= run<ReconcileTest>(argc, argv);
    status |= run<ResponseMergeTest>(argc, argv);
    status |= run<PairingTest>(argc, argv);
    status |= run<ServerConnectionTest>(argc, argv);
    status |= run<DocumentFileTest>(argc, argv);
    status |= run<DocumentSessionTest>(argc, argv);
    status |= run<RosterChangeQueueTest>(argc, argv);

    return status;
}
