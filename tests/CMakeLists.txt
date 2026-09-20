# Tests CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Test)

# Optionally use Google Test if available
# find_package(GTest)

enable_testing()

# ============================================================================
# Test Executable
# ============================================================================

set(TEST_SOURCES
    # Model tests
    # models/PersonTest.cpp
    # models/FamilyTest.cpp
    # models/DocumentTest.cpp
    # models/TeamTest.cpp
    # models/TagTest.cpp

    # Command tests
    # commands/FamilyCommandsTest.cpp
    # commands/CommandHistoryTest.cpp

    # Service tests
    # services/JsonServiceTest.cpp
    # services/FilterServiceTest.cpp
)

# Only build if we have test sources
if(TEST_SOURCES)
    add_executable(WardPlanningTests
        ${TEST_SOURCES}
    )

    target_link_libraries(WardPlanningTests PRIVATE
        WardPlanningLib
        Qt6::Test
    )

    # If using Google Test
    # if(GTest_FOUND)
    #     target_link_libraries(WardPlanningTests PRIVATE GTest::gtest GTest::gtest_main)
    # endif()

    # Add tests to CTest
    add_test(NAME WardPlanningTests COMMAND WardPlanningTests)
endif()

# ============================================================================
# Individual Test Registration (when using Qt Test)
# ============================================================================

# Example of registering individual tests:
# add_test(NAME PersonTest COMMAND WardPlanningTests PersonTest)
# add_test(NAME FamilyTest COMMAND WardPlanningTests FamilyTest)
