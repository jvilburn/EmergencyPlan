# The instance lock's tests, which start copies of this executable as separate processes.

add_executable(WardPlanningLockTests
    main.cpp
    LockTest.h
    LockTest.cpp
)

target_link_libraries(WardPlanningLockTests PRIVATE
    WardPlanningLib
    Qt6::Test
)

add_test(NAME WardPlanningLockTests COMMAND WardPlanningLockTests)
