# Tests CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Test)

enable_testing()

# ============================================================================
# Test Executable
# ============================================================================

# One runner: TestMain.cpp holds the only main() and runs every suite in turn.
set(TEST_SOURCES
    TestMain.cpp
    models/DocumentParseTest.cpp
    models/SealingSetTest.cpp
    models/ModelParsersTest.cpp
    models/ModelSamples.cpp
    services/AppSettingsTest.cpp
    services/CryptoLinkTest.cpp
    services/CryptoTypesTest.cpp
    services/DocumentCryptoTest.cpp
    services/DocumentFileTest.cpp
    services/DocumentHeaderTest.cpp
    services/DocumentLibraryTest.cpp
    services/DocumentManagerTest.cpp
    services/DocumentSessionTest.cpp
    services/FakeServer.cpp
    services/FakeServerTest.cpp
    services/FileOpenRouterTest.cpp
    services/HttpServerApiTest.cpp
    services/FormatVectorsTest.cpp
    services/IdentityFileTest.cpp
    services/IdentityServiceTest.cpp
    services/PendingRequestsTest.cpp
    services/ReconcileTest.cpp
    services/ResponseMergeTest.cpp
    services/SealingQueueTest.cpp
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
