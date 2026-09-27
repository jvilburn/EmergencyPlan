# Tests CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Test)

enable_testing()

# ============================================================================
# Test Executable
# ============================================================================

# One runner: TestMain.cpp holds the only main() and runs every suite in turn.
set(TEST_SOURCES
    TestMain.cpp
    models/AccessRosterTest.cpp
    services/AccessControlTest.cpp
    services/ChainTestSupport.cpp
    services/CryptoLinkTest.cpp
    services/CryptoTypesTest.cpp
    services/DocumentChainTest.cpp
    services/DocumentCryptoTest.cpp
    services/DocumentHeaderTest.cpp
    services/FakeServer.cpp
    services/FakeServerTest.cpp
    services/ServerConformanceTest.cpp
    services/ServerTestSupport.cpp
    services/SmokeTest.cpp
)

add_executable(WardPlanningTests
    ${TEST_SOURCES}
)

target_link_libraries(WardPlanningTests PRIVATE
    WardPlanningLib
    Qt6::Test
)

add_test(NAME WardPlanningTests COMMAND WardPlanningTests)
