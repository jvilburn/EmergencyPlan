cmake_minimum_required(VERSION 3.21)

project(EmergencyPlan
    VERSION 1.0.0
    DESCRIPTION "Emergency Plan - Ward/Stake Emergency Response Planning"
    LANGUAGES CXX
)

# ============================================================================
# C++ Standard Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Qt Configuration
# ============================================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 - adjust path if needed for your installation
# set(CMAKE_PREFIX_PATH "C:/Qt/6.6.0/msvc2019_64" CACHE PATH "Qt installation path")

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Svg
    Pdf
    Concurrent
    Test
)

# ============================================================================
# MuPDF for PDF text extraction (from vcpkg, no cmake config provided)
# ============================================================================
find_library(MUPDF_LIBRARY NAMES libmupdf mupdf REQUIRED)
find_path(MUPDF_INCLUDE_DIR mupdf/fitz.h REQUIRED)
# MuPDF dependencies
find_library(FREETYPE_LIBRARY NAMES freetype REQUIRED)
find_library(JPEG_LIBRARY NAMES jpeg REQUIRED)
find_library(ZLIB_LIBRARY NAMES zlib z REQUIRED)
find_library(OPENJP2_LIBRARY NAMES openjp2 REQUIRED)
find_library(HARFBUZZ_LIBRARY NAMES harfbuzz)
find_library(JBIG2DEC_LIBRARY NAMES jbig2dec)
find_library(GUMBO_LIBRARY NAMES gumbo REQUIRED)
find_library(PNG_LIBRARY NAMES libpng16 png16 png REQUIRED)
find_library(BZ2_LIBRARY NAMES bz2 bzip2)
find_library(BROTLIDEC_LIBRARY NAMES brotlidec)
find_library(BROTLICOMMON_LIBRARY NAMES brotlicommon)
message(STATUS "MuPDF library: ${MUPDF_LIBRARY}")


# ============================================================================
# Compiler Warnings
# ============================================================================
if(MSVC)
    add_compile_options(/W4 /WX-)
    # Disable specific warnings if needed:
    # add_compile_options(/wd4100)  # unreferenced formal parameter
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# Output Directories
# ============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# Source Files
# ============================================================================

# Collect all source files
set(MODEL_SOURCES
    src/models/Name.cpp
    src/models/Phone.cpp
    src/models/Address.cpp
    src/models/Gender.cpp
    src/models/Email.cpp
    src/models/Birthday.cpp
    src/models/Calling.cpp
    src/models/Person.cpp
    src/models/Family.cpp
    src/models/Team.cpp
    src/models/Tag.cpp
    src/models/Ward.cpp
    src/models/Stake.cpp
    src/models/DocumentMetadata.cpp
    src/models/Document.cpp
    src/models/MinisteringGroup.cpp
    src/models/MinisteringDistrict.cpp
    src/models/ResourceCategory.cpp
    src/models/ResourceType.cpp
)

set(MODEL_HEADERS
    src/models/Name.h
    src/models/Phone.h
    src/models/Address.h
    src/models/Gender.h
    src/models/Email.h
    src/models/Birthday.h
    src/models/Calling.h
    src/models/ViewMode.h
    src/models/MarkerDecorationType.h
    src/models/ResourceLevel.h
    src/models/Person.h
    src/models/Family.h
    src/models/Team.h
    src/models/Tag.h
    src/models/Ward.h
    src/models/Stake.h
    src/models/DocumentMetadata.h
    src/models/Document.h
    src/models/MinisteringGroup.h
    src/models/MinisteringDistrict.h
    src/models/ResourceCategory.h
    src/models/ResourceType.h
)

set(COMMAND_SOURCES
    src/commands/CommandHistory.cpp
    src/commands/FamilyCommands.cpp
    src/commands/ImportWardDirectoryCommand.cpp
    src/commands/ImportEQMinisteringCommand.cpp
    src/commands/ImportRSMinisteringCommand.cpp
    src/commands/TeamCommands.cpp
    src/commands/MinisteringCommands.cpp
    src/commands/TagCommands.cpp
    src/commands/ResourceCommands.cpp
    # src/commands/ImportCommands.cpp
)

set(COMMAND_HEADERS
    src/commands/Command.h
    src/commands/CommandHistory.h
    src/commands/FamilyCommands.h
    src/commands/ImportWardDirectoryCommand.h
    src/commands/ImportEQMinisteringCommand.h
    src/commands/ImportRSMinisteringCommand.h
    src/commands/TeamCommands.h
    src/commands/MinisteringCommands.h
    src/commands/TagCommands.h
    src/commands/ResourceCommands.h
    # src/commands/ImportCommands.h
)

set(SERVICE_SOURCES
    src/services/DocumentManager.cpp
    src/services/JsonService.cpp
    src/services/PdfExtractor.cpp
    src/services/PdfFieldUtils.cpp
    src/services/PersonMatching.cpp
    src/services/WardDirectoryImportService.cpp
    src/services/WardDirectoryPdfParser.cpp
    src/services/MinisteringPdfParser.cpp
    src/services/MinisteringImportService.cpp
    src/services/QoiCodec.cpp
    src/services/TileDiskCacheService.cpp
    src/services/TileFetchService.cpp
    src/services/TileService.cpp
    src/services/UnitLookupService.cpp
    src/services/GeocodingService.cpp
    src/services/BackgroundGeocodingService.cpp
    src/services/GeoLocation.cpp
    # src/services/StatisticsService.cpp
    # src/services/ResourceService.cpp
    # src/services/SearchService.cpp
    # src/services/PreferencesService.cpp
    # src/services/PdfReportService.cpp
)

set(SERVICE_HEADERS
    src/services/DocumentManager.h
    src/services/JsonService.h
    src/services/PdfExtractor.h
    src/services/PersonMatching.h
    src/services/WardDirectoryImportService.h
    src/services/WardDirectoryPdfParser.h
    src/services/MinisteringPdfParser.h
    src/services/MinisteringImportService.h
    src/services/QoiCodec.h
    src/services/TileDiskCacheService.h
    src/services/TileFetchService.h
    src/services/TileTypes.h
    src/services/TileService.h
    src/services/UnitLookupService.h
    src/services/GeocodingService.h
    src/services/BackgroundGeocodingService.h
    src/services/GeoLocation.h
    # src/services/StatisticsService.h
    # src/services/ResourceService.h
    # src/services/SearchService.h
    # src/services/PreferencesService.h
    # src/services/PdfReportService.h
)

set(VIEWMODEL_SOURCES
    src/viewmodels/MapViewModel.cpp
)

set(VIEWMODEL_HEADERS
    src/viewmodels/MapViewModel.h
)

set(WIDGET_SOURCES
    src/widgets/ActionButtonsWidget.cpp
    src/widgets/CallingsEditor.cpp
    src/widgets/FamilyEditPanel.cpp
    src/widgets/MainWindow.cpp
    src/widgets/MemberAccordion.cpp
    src/widgets/MemberEditor.cpp
    src/widgets/MapWidget.cpp
    src/widgets/MarkerRenderer.cpp
    src/widgets/MinisteringView.cpp
    src/widgets/SlippyMapMath.cpp
    src/widgets/UnmappedPanel.cpp
    src/widgets/WardListDialog.cpp
    src/widgets/WardListView.cpp
    src/widgets/shared/FilterableListWidget.cpp
    src/widgets/shared/SearchField.cpp
    # src/widgets/Sidebar.cpp
    # src/widgets/MinisteringSidebar.cpp
    # src/widgets/EmergencyView.cpp
    # src/widgets/ResourceView.cpp
    # src/widgets/TagsView.cpp
    # src/widgets/TeamsView.cpp
    # src/widgets/SelectionView.cpp
    # src/widgets/shared/FilterChips.cpp
    # src/widgets/shared/FamilyTile.cpp
    # src/widgets/shared/PersonTile.cpp
    # src/widgets/shared/BadgeWidget.cpp
    # src/widgets/dialogs/FamilyDialog.cpp
    # src/widgets/dialogs/PersonForm.cpp
    # src/widgets/dialogs/VisitDialog.cpp
    # src/widgets/dialogs/ImportDialog.cpp
    # src/widgets/dialogs/StatisticsDialog.cpp
    # src/widgets/dialogs/TagDialog.cpp
    # src/widgets/dialogs/ResourceDialog.cpp
)

set(WIDGET_HEADERS
    src/widgets/ActionButtonsWidget.h
    src/widgets/CallingsEditor.h
    src/widgets/FamilyEditPanel.h
    src/widgets/MainWindow.h
    src/widgets/MemberAccordion.h
    src/widgets/MemberEditor.h
    src/widgets/MapHighlightProvider.h
    src/widgets/MapWidget.h
    src/widgets/MarkerRenderer.h
    src/widgets/MinisteringView.h
    src/widgets/SlippyMapMath.h
    src/widgets/UnmappedPanel.h
    src/widgets/WardListDialog.h
    src/widgets/WardListView.h
    src/widgets/shared/FilterableListWidget.h
    src/widgets/shared/SearchField.h
    # src/widgets/Sidebar.h
    # src/widgets/MinisteringSidebar.h
    # src/widgets/EmergencyView.h
    # src/widgets/ResourceView.h
    # src/widgets/TagsView.h
    # src/widgets/TeamsView.h
    # src/widgets/SelectionView.h
    # src/widgets/shared/FilterChips.h
    # src/widgets/shared/FamilyTile.h
    # src/widgets/shared/PersonTile.h
    # src/widgets/shared/BadgeWidget.h
    # src/widgets/dialogs/FamilyDialog.h
    # src/widgets/dialogs/PersonForm.h
    # src/widgets/dialogs/VisitDialog.h
    # src/widgets/dialogs/ImportDialog.h
    # src/widgets/dialogs/StatisticsDialog.h
    # src/widgets/dialogs/TagDialog.h
    # src/widgets/dialogs/ResourceDialog.h
)

set(LISTMODEL_SOURCES
    src/listmodels/Filter.cpp
    src/listmodels/FamilyListModel.cpp
    src/listmodels/FamilyTreeModel.cpp
    src/listmodels/PersonListModel.cpp
    src/listmodels/TeamListModel.cpp
    src/listmodels/TagListModel.cpp
    # src/listmodels/DistrictTreeModel.cpp
    # src/listmodels/ResourceTreeModel.cpp
)

set(LISTMODEL_HEADERS
    src/listmodels/Filter.h
    src/listmodels/FamilyListModel.h
    src/listmodels/FamilyTreeModel.h
    src/listmodels/PersonListModel.h
    src/listmodels/TeamListModel.h
    src/listmodels/TagListModel.h
    # src/listmodels/DistrictTreeModel.h
    # src/listmodels/ResourceTreeModel.h
)

# ============================================================================
# Library Target (for testing)
# ============================================================================
add_library(EmergencyPlanLib STATIC
    ${MODEL_SOURCES}
    ${MODEL_HEADERS}
    ${COMMAND_SOURCES}
    ${COMMAND_HEADERS}
    ${SERVICE_SOURCES}
    ${SERVICE_HEADERS}
    ${VIEWMODEL_SOURCES}
    ${VIEWMODEL_HEADERS}
    ${WIDGET_SOURCES}
    ${WIDGET_HEADERS}
    ${LISTMODEL_SOURCES}
    ${LISTMODEL_HEADERS}
)

target_include_directories(EmergencyPlanLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/commands
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/src/listmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

# Mark third-party includes as SYSTEM to suppress warnings from their headers
target_include_directories(EmergencyPlanLib SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty
    ${MUPDF_INCLUDE_DIR}
)

target_link_libraries(EmergencyPlanLib PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::Pdf
    Qt6::Concurrent
    ${MUPDF_LIBRARY}
    ${FREETYPE_LIBRARY}
    ${JPEG_LIBRARY}
    ${ZLIB_LIBRARY}
    ${OPENJP2_LIBRARY}
    ${GUMBO_LIBRARY}
)

if(HARFBUZZ_LIBRARY)
    target_link_libraries(EmergencyPlanLib PUBLIC ${HARFBUZZ_LIBRARY})
endif()
if(JBIG2DEC_LIBRARY)
    target_link_libraries(EmergencyPlanLib PUBLIC ${JBIG2DEC_LIBRARY})
endif()
if(PNG_LIBRARY)
    target_link_libraries(EmergencyPlanLib PUBLIC ${PNG_LIBRARY})
endif()
if(BZ2_LIBRARY)
    target_link_libraries(EmergencyPlanLib PUBLIC ${BZ2_LIBRARY})
endif()
if(BROTLIDEC_LIBRARY)
    target_link_libraries(EmergencyPlanLib PUBLIC ${BROTLIDEC_LIBRARY})
endif()
if(BROTLICOMMON_LIBRARY)
    target_link_libraries(EmergencyPlanLib PUBLIC ${BROTLICOMMON_LIBRARY})
endif()

target_compile_definitions(EmergencyPlanLib PUBLIC HAVE_MUPDF)


# ============================================================================
# Main Executable
# ============================================================================
qt_add_executable(EmergencyPlan
    src/main.cpp
)

target_link_libraries(EmergencyPlan PRIVATE
    EmergencyPlanLib
)

# ============================================================================
# Resources
# ============================================================================
qt_add_resources(EmergencyPlan "icons"
    PREFIX "/"
    BASE "src/resources"
    FILES
        src/resources/icons/map.svg
        src/resources/icons/satellite.svg
        src/resources/icons/recenter.svg
)

# ============================================================================
# Platform-Specific Settings
# ============================================================================

# --- Windows ---
if(WIN32)
    set_target_properties(EmergencyPlan PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    # Windows application icon
    # target_sources(EmergencyPlan PRIVATE packaging/windows/EmergencyPlan.rc)

    # Windows-specific: Include skeuomorphic stylesheet in resources
    # The stylesheet is loaded at runtime on Windows only
    message(STATUS "Windows build: Skeuomorphic styling enabled")
endif()

# --- macOS ---
if(APPLE)
    set_target_properties(EmergencyPlan PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.church.emergencyplan"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_NAME "Emergency Plan"
    )

    # macOS-specific: Use native styling (no custom stylesheet)
    # Qt automatically uses native Aqua controls
    message(STATUS "macOS build: Native Aqua styling enabled")

    # Enable Retina/HiDPI support
    set_target_properties(EmergencyPlan PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "EmergencyPlan.icns"
    )

    # Copy icon to bundle
    # set(MACOS_ICON ${CMAKE_SOURCE_DIR}/packaging/macos/EmergencyPlan.icns)
    # set_source_files_properties(${MACOS_ICON} PROPERTIES
    #     MACOSX_PACKAGE_LOCATION "Resources"
    # )
    # target_sources(EmergencyPlan PRIVATE ${MACOS_ICON})
endif()

# --- Cross-platform definitions ---
target_compile_definitions(EmergencyPlan PRIVATE
    $<$<PLATFORM_ID:Windows>:PLATFORM_WINDOWS>
    $<$<PLATFORM_ID:Darwin>:PLATFORM_MACOS>
    $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX>
)

# ============================================================================
# Tools
# ============================================================================
option(BUILD_TOOLS "Build development tools" ON)

if(BUILD_TOOLS)
    # PDF dump tool for analyzing MuPDF extraction
    add_executable(pdf_dump tools/pdf_dump.cpp)
    target_link_libraries(pdf_dump PRIVATE EmergencyPlanLib)

    # Test import tool for WardDirectoryImportService
    add_executable(test_import tools/test_import.cpp)
    target_link_libraries(test_import PRIVATE EmergencyPlanLib)
endif()

# ============================================================================
# Testing
# ============================================================================
option(BUILD_TESTS "Build test suite" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS EmergencyPlan
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install Qt runtime (Windows)
if(WIN32)
    # qt_generate_deploy_app_script(
    #     TARGET EmergencyPlan
    #     OUTPUT_SCRIPT deploy_script
    #     NO_UNSUPPORTED_PLATFORM_ERROR
    # )
    # install(SCRIPT ${deploy_script})
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "=== Emergency Plan Qt Configuration ===")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt Version: ${Qt6_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "")
