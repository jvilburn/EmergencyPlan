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
    models/DocumentParseTest.cpp
    models/ModelParsersTest.cpp
    models/ModelSamples.cpp
    services/AccessControlTest.cpp
    services/AppSettingsTest.cpp
    services/ChainTestSupport.cpp
    services/CryptoLinkTest.cpp
    services/CryptoTypesTest.cpp
    services/DocumentChainTest.cpp
    services/DocumentCryptoTest.cpp
    services/DocumentFileTest.cpp
    services/DocumentHeaderTest.cpp
    services/DocumentLibraryTest.cpp
    services/DocumentSessionTest.cpp
    services/FakeServer.cpp
    services/FakeServerTest.cpp
    services/FileOpenRouterTest.cpp
    services/FormatVectorsTest.cpp
    services/IdentityFileTest.cpp
    services/IdentityServiceTest.cpp
    services/PairingTest.cpp
    services/ReconcileTest.cpp
    services/ResponseMergeTest.cpp
    services/RosterChangeQueueTest.cpp
    services/ServerConformanceTest.cpp
    services/ServerConnectionTest.cpp
    services/ServerTestSupport.cpp
    services/SessionTestSupport.cpp
    services/SmokeTest.cpp
    services/WardDirectoryImportTest.cpp
)

add_executable(WardPlanningTests
    ${TEST_SOURCES}
)

target_link_libraries(WardPlanningTests PRIVATE
    WardPlanningLib
    Qt6::Test
)

target_include_directories(WardPlanningTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/services
)

add_test(NAME WardPlanningTests COMMAND WardPlanningTests)
