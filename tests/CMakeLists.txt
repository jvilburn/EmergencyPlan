# Tests CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(WardPlanningTests
    main.cpp
    services/TestWard.h
    services/TestWard.cpp
    services/TestCommands.h
    services/TestCommands.cpp
    services/FakeGeocoder.h
    services/FakeGeocoder.cpp
    services/FakeUnitLookup.h
    services/FakeUnitLookup.cpp
    services/TestSession.h
    services/TestSession.cpp
    services/FakeWardStore.h
    services/FakeWardStore.cpp
    services/TestClient.h
    services/TestClient.cpp
    services/EveryField.h
    services/EveryField.cpp
    services/WardFileEdits.h
    services/WardFileEdits.cpp
    services/PdfText.h
    services/PdfText.cpp
    services/CommandSamples.h
    services/CommandSamples.cpp
    services/FamilyOrderTest.h
    services/FamilyOrderTest.cpp
    services/FieldChangeTest.h
    services/FieldChangeTest.cpp
    services/UpdateFamilyCommandTest.h
    services/UpdateFamilyCommandTest.cpp
    services/CommandRoundTripTest.h
    services/CommandRoundTripTest.cpp
    services/WardSessionTest.h
    services/WardSessionTest.cpp
    services/AutomaticUpdatesTest.h
    services/AutomaticUpdatesTest.cpp
    services/TeamCommandsTest.h
    services/TeamCommandsTest.cpp
    services/EmergencyAssetCommandsTest.h
    services/EmergencyAssetCommandsTest.cpp
    services/EmergencyCommandsTest.h
    services/EmergencyCommandsTest.cpp
    services/ImportCommandsTest.h
    services/ImportCommandsTest.cpp
    services/ReferenceCleanupTest.h
    services/ReferenceCleanupTest.cpp
    services/FilterTest.h
    services/FilterTest.cpp
    services/PersonJsonTest.h
    services/PersonJsonTest.cpp
    services/BirthdayTest.h
    services/BirthdayTest.cpp
    services/MemberEditorTest.h
    services/MemberEditorTest.cpp
    services/FamilyEditPanelTest.h
    services/FamilyEditPanelTest.cpp
    services/JsonReaderTest.h
    services/JsonReaderTest.cpp
    services/IdentityTest.h
    services/IdentityTest.cpp
    services/WardFileTest.h
    services/WardFileTest.cpp
    services/FileWardStoreTest.h
    services/FileWardStoreTest.cpp
    services/WardSessionSavingTest.h
    services/WardSessionSavingTest.cpp
    services/StrictReadersTest.h
    services/StrictReadersTest.cpp
)

target_include_directories(WardPlanningTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/services
)

target_link_libraries(WardPlanningTests PRIVATE
    EmergencyPlanLib
    Qt6::Test
)

add_test(NAME WardPlanningTests COMMAND WardPlanningTests)
