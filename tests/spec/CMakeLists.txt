# The format reader built from docs/FORMAT.md alone, with no Qt and none of the app's code.

find_package(nlohmann_json CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

add_executable(WardPlanningSpecReader
    main.cpp
    Fields.h
    Fields.cpp
    Envelope.h
    Envelope.cpp
    Payload.h
    Payload.cpp
)

target_link_libraries(WardPlanningSpecReader PRIVATE
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
    unofficial-sodium::sodium
)
