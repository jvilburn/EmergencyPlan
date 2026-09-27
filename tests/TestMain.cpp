#include "models/AccessRosterTest.h"
#include "services/AccessControlTest.h"
#include "services/CryptoLinkTest.h"
#include "services/CryptoTypesTest.h"
#include "services/DocumentChainTest.h"
#include "services/DocumentCryptoTest.h"
#include "services/DocumentHeaderTest.h"
#include "services/FakeServer.h"
#include "services/FakeServerTest.h"
#include "services/ServerConformanceTest.h"
#include "services/SmokeTest.h"

#include "DocumentCrypto.h"

#include <QApplication>
#include <QStandardPaths>
#include <QtTest>

#include <memory>

namespace
{

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
    status |= run<DocumentChainTest>(argc, argv);
    status |= run<AccessRosterTest>(argc, argv);
    status |= run<AccessControlTest>(argc, argv);
    status |= run<FakeServerTest>(argc, argv);
    status |= run<ServerConformanceTest>(
        argc,
        argv,
        []() -> std::unique_ptr<ServerContract::ServerApi>
        {
            return std::make_unique<FakeServer>();
        });

    return status;
}
