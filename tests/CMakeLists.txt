# Tests CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(WardPlanningTests
    main.cpp
    services/TestWard.h
    services/TestWard.cpp
    services/TestCommands.h
    services/TestCommands.cpp
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
    services/PersonJsonTest.h
    services/PersonJsonTest.cpp
    services/BirthdayTest.h
    services/BirthdayTest.cpp
    services/MemberEditorTest.h
    services/MemberEditorTest.cpp
    services/FamilyEditPanelTest.h
    services/FamilyEditPanelTest.cpp
)

target_include_directories(WardPlanningTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/services
)

target_link_libraries(WardPlanningTests PRIVATE
    EmergencyPlanLib
    Qt6::Test
)

add_test(NAME WardPlanningTests COMMAND WardPlanningTests)
