cmake_minimum_required(VERSION 3.21)

project(WardPlanning
    VERSION 2.0.0
    DESCRIPTION "Ward Planning - ward planning and emergency response"
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

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Svg
    Pdf
    Concurrent
    PrintSupport
)

# ============================================================================
# MuPDF for PDF text extraction (from vcpkg, no cmake config provided)
# ============================================================================
find_library(MUPDF_LIBRARY NAMES libmupdf mupdf REQUIRED)
find_path(MUPDF_INCLUDE_DIR mupdf/fitz.h REQUIRED)
# MuPDF dependencies
find_library(FREETYPE_LIBRARY NAMES freetype REQUIRED)
find_library(JPEG_LIBRARY NAMES jpeg REQUIRED)
find_library(ZLIB_LIBRARY NAMES zlib z zs zsd REQUIRED)
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
# libsodium for the ward file's encryption and the device keys (from vcpkg)
# ============================================================================
find_package(unofficial-sodium CONFIG REQUIRED)


# ============================================================================
# Compiler Warnings
# ============================================================================
if(MSVC)
    add_compile_options(/W4 /WX-)
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
    src/models/Coordinates.cpp
    src/models/Team.cpp
    src/models/Ward.cpp
    src/models/Stake.cpp
    src/models/DocumentMetadata.cpp
    src/models/Document.cpp
    src/models/MinisteringGroup.cpp
    src/models/MinisteringDistrict.cpp
    src/models/EmergencyAsset.cpp
    src/models/DocumentChange.cpp
    src/models/EmergencyResponse.cpp
    src/models/Id.cpp
    src/models/AccessList.cpp
)

set(MODEL_HEADERS
    src/models/Name.h
    src/models/Phone.h
    src/models/Address.h
    src/models/Gender.h
    src/models/Email.h
    src/models/Birthday.h
    src/models/Calling.h
    src/models/Person.h
    src/models/Family.h
    src/models/Coordinates.h
    src/models/Team.h
    src/models/Ward.h
    src/models/Stake.h
    src/models/DocumentMetadata.h
    src/models/Document.h
    src/models/MinisteringGroup.h
    src/models/MinisteringDistrict.h
    src/models/EmergencyAsset.h
    src/models/ResponseArea.h
    src/models/DocumentChange.h
    src/models/Id.h
    src/models/EmergencyResponse.h
    src/models/AccessList.h
)

set(COMMAND_SOURCES
    src/commands/Command.cpp
    src/commands/FieldChangeJson.cpp
    src/commands/FamilyChange.cpp
    src/commands/CommandRegistry.cpp
    src/commands/FamilyCommands.cpp
    src/commands/AddressCoordinatesCommands.cpp
    src/commands/TeamCommands.cpp
    src/commands/EmergencyAssetCommands.cpp
    src/commands/EmergencyCommands.cpp
    src/commands/ImportWardDirectoryCommand.cpp
    src/commands/ImportMinisteringCommand.cpp
    src/commands/AccessRules.cpp
    src/commands/AccessCommands.cpp
    src/commands/ReferenceCleanup.cpp
)

set(COMMAND_HEADERS
    src/commands/Command.h
    src/commands/FieldChange.h
    src/commands/FieldChangeJson.h
    src/commands/FamilyChange.h
    src/commands/CommandRegistry.h
    src/commands/FamilyCommands.h
    src/commands/AddressCoordinatesCommands.h
    src/commands/TeamCommands.h
    src/commands/EmergencyAssetCommands.h
    src/commands/EmergencyCommands.h
    src/commands/ImportWardDirectoryCommand.h
    src/commands/ImportMinisteringCommand.h
    src/commands/AccessRules.h
    src/commands/AccessCommands.h
    src/commands/ReferenceCleanup.h
)

set(SERVICE_SOURCES
    src/services/DocumentManager.cpp
    src/services/PdfExtractor.cpp
    src/services/PersonMatching.cpp
    src/services/WardDirectoryImportService.cpp
    src/services/WardDirectoryPdfParser.cpp
    src/services/MinisteringPdfParser.cpp
    src/services/MinisteringImportService.cpp
    src/services/TileCoverage.cpp
    src/services/TileDiskCacheService.cpp
    src/services/TileFetchService.cpp
    src/services/TileService.cpp
    src/services/UnitLookup.cpp
    src/services/UnitLookupService.cpp
    src/services/Geocoder.cpp
    src/services/GeocodingService.cpp
    src/services/GeoLocation.cpp
    src/services/EmergencyManager.cpp
    src/services/ReportGenerator.cpp
    src/services/Journal.cpp
    src/services/WardSession.cpp
    src/services/AutomaticUpdates.cpp
    src/services/Base64Url.cpp
    src/services/Crypto.cpp
    src/services/JsonReader.cpp
    src/services/Identity.cpp
    src/services/KeyFile.cpp
    src/services/DeviceState.cpp
    src/services/WardPayload.cpp
    src/services/WardHeader.cpp
    src/services/WardFile.cpp
    src/services/FileWardStore.cpp
    src/services/AppData.cpp
    src/services/AppSettings.cpp
    src/services/WardList.cpp
    src/services/ShowInFolder.cpp
    src/services/UpdateFile.cpp
    src/services/InvitationFile.cpp
    src/services/UserFolder.cpp
    src/services/InstanceLock.cpp
)

set(SERVICE_HEADERS
    src/services/DocumentManager.h
    src/services/PdfExtractor.h
    src/services/PersonMatching.h
    src/services/WardDirectoryImportService.h
    src/services/WardDirectoryPdfParser.h
    src/services/MinisteringPdfParser.h
    src/services/MinisteringImportService.h
    src/services/TileCoverage.h
    src/services/TileDiskCacheService.h
    src/services/TileFetchService.h
    src/services/TileTypes.h
    src/services/TileService.h
    src/services/UnitLookup.h
    src/services/UnitLookupService.h
    src/services/Geocoder.h
    src/services/GeocodingService.h
    src/services/GeoLocation.h
    src/services/EmergencyManager.h
    src/services/ReportGenerator.h
    src/services/Journal.h
    src/services/WardSession.h
    src/services/AutomaticUpdates.h
    src/services/Base64Url.h
    src/services/Crypto.h
    src/services/JsonReader.h
    src/services/Identity.h
    src/services/KeyFile.h
    src/services/DeviceState.h
    src/services/WardPayload.h
    src/services/WardHeader.h
    src/services/WardFile.h
    src/services/WardStore.h
    src/services/FileWardStore.h
    src/services/AppData.h
    src/services/AppSettings.h
    src/services/WardList.h
    src/services/ShowInFolder.h
    src/services/UpdateFile.h
    src/services/InvitationFile.h
    src/services/UserFolder.h
    src/services/InstanceLock.h
)

set(VIEWMODEL_SOURCES
    src/viewmodels/MapViewModel.cpp
)

set(VIEWMODEL_HEADERS
    src/viewmodels/MapViewModel.h
)

set(WIDGET_SOURCES
    src/widgets/ActionButtonsWidget.cpp
    src/widgets/ChoiceDialog.cpp
    src/widgets/ContactAttemptDialog.cpp
    src/widgets/NotifyDialog.cpp
    src/widgets/PersonCombo.cpp
    src/widgets/TaskDialog.cpp
    src/widgets/TaskListView.cpp
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
    src/widgets/PeoplePickerDialog.cpp
    src/widgets/OpenWardDialog.cpp
    src/widgets/SettingsDialog.cpp
    src/widgets/FamilyListView.cpp
    src/widgets/shared/FilterBar.cpp
    src/widgets/shared/FlowLayout.cpp
    src/widgets/shared/FilterChip.cpp
    src/widgets/shared/SearchField.cpp
    src/widgets/NeedsSubView.cpp
    src/widgets/SelectionPreservingTreeView.cpp
    src/widgets/SculptedItemDelegate.cpp
    src/widgets/EmergencyAssetView.cpp
    src/widgets/EmergencyActions.cpp
    src/widgets/EmergencyBanner.cpp
    src/widgets/EmergencyProgressBar.cpp
    src/widgets/SidebarWidget.cpp
    src/widgets/MinisteringTreeView.cpp
    src/widgets/UnassignedTreeView.cpp
    src/widgets/MinisteringTabView.cpp
    src/widgets/TeamsView.cpp
)

set(WIDGET_HEADERS
    src/widgets/ActionButtonsWidget.h
    src/widgets/ChoiceDialog.h
    src/widgets/ContactAttemptDialog.h
    src/widgets/NotifyDialog.h
    src/widgets/PersonCombo.h
    src/widgets/TaskDialog.h
    src/widgets/TaskListView.h
    src/widgets/CallingsEditor.h
    src/widgets/FamilyEditPanel.h
    src/widgets/MainWindow.h
    src/widgets/MemberAccordion.h
    src/widgets/MemberEditor.h
    src/widgets/FamilyMarkerProvider.h
    src/widgets/MapWidget.h
    src/widgets/MarkerRenderer.h
    src/widgets/MinisteringView.h
    src/widgets/SlippyMapMath.h
    src/widgets/UnmappedPanel.h
    src/widgets/PeoplePickerDialog.h
    src/widgets/OpenWardDialog.h
    src/widgets/SettingsDialog.h
    src/widgets/FamilyListView.h
    src/widgets/shared/FilterBar.h
    src/widgets/shared/FlowLayout.h
    src/widgets/shared/FilterChip.h
    src/widgets/shared/SearchField.h
    src/widgets/NeedsSubView.h
    src/widgets/SelectionPreservingTreeView.h
    src/widgets/SculptedItemDelegate.h
    src/widgets/EmergencyAssetView.h
    src/widgets/EmergencyActions.h
    src/widgets/EmergencyBanner.h
    src/widgets/EmergencyProgressBar.h
    src/widgets/SidebarWidget.h
    src/widgets/TeamsView.h
)

set(LISTMODEL_SOURCES
    src/listmodels/BaseTreeModel.cpp
    src/listmodels/EmergencyAssetModel.cpp
    src/listmodels/Filter.cpp
    src/listmodels/FamilyTreeModel.cpp
    src/listmodels/ContactIcons.cpp
    src/listmodels/MinisteringModel.cpp
    src/listmodels/StatusIcons.cpp
    src/listmodels/NeedsModel.cpp
    src/listmodels/PersonTreeModel.cpp
    src/listmodels/UnassignedMinisteringModel.cpp
    src/listmodels/TaskListModel.cpp
    src/listmodels/TeamsTreeModel.cpp
)

set(LISTMODEL_HEADERS
    src/listmodels/BaseTreeModel.h
    src/listmodels/EmergencyAssetModel.h
    src/listmodels/Filter.h
    src/listmodels/FamilyTreeModel.h
    src/listmodels/MinisteringModel.h
    src/listmodels/StatusIcons.h
    src/listmodels/NeedsModel.h
    src/listmodels/PersonTreeModel.h
    src/listmodels/TaskListModel.h
    src/listmodels/UnassignedMinisteringModel.h
    src/listmodels/TeamsTreeModel.h
)

# ============================================================================
# Library Target (for testing)
# ============================================================================
add_library(WardPlanningLib STATIC
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

target_include_directories(WardPlanningLib PUBLIC
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
target_include_directories(WardPlanningLib SYSTEM PUBLIC
    ${MUPDF_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/qoi
)

target_link_libraries(WardPlanningLib PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::Pdf
    Qt6::Concurrent
    Qt6::PrintSupport
    ${MUPDF_LIBRARY}
    ${FREETYPE_LIBRARY}
    ${JPEG_LIBRARY}
    ${ZLIB_LIBRARY}
    ${OPENJP2_LIBRARY}
    ${GUMBO_LIBRARY}
    unofficial-sodium::sodium
)

if(HARFBUZZ_LIBRARY)
    target_link_libraries(WardPlanningLib PUBLIC ${HARFBUZZ_LIBRARY})
endif()
if(JBIG2DEC_LIBRARY)
    target_link_libraries(WardPlanningLib PUBLIC ${JBIG2DEC_LIBRARY})
endif()
if(PNG_LIBRARY)
    target_link_libraries(WardPlanningLib PUBLIC ${PNG_LIBRARY})
endif()
if(BZ2_LIBRARY)
    target_link_libraries(WardPlanningLib PUBLIC ${BZ2_LIBRARY})
endif()
if(BROTLIDEC_LIBRARY)
    target_link_libraries(WardPlanningLib PUBLIC ${BROTLIDEC_LIBRARY})
endif()
if(BROTLICOMMON_LIBRARY)
    target_link_libraries(WardPlanningLib PUBLIC ${BROTLICOMMON_LIBRARY})
endif()


# ============================================================================
# Main Executable
# ============================================================================
qt_add_executable(WardPlanning
    src/main.cpp
    src/resources/app_icon.rc
)

target_link_libraries(WardPlanning PRIVATE
    WardPlanningLib
)

# ============================================================================
# Resources
# ============================================================================
qt_add_resources(WardPlanning "icons"
    PREFIX "/"
    BASE "src/resources"
    FILES
        src/resources/icons/map.svg
        src/resources/icons/satellite.svg
        src/resources/icons/recenter.svg
        src/resources/icons/zoom-in.svg
        src/resources/icons/zoom-out.svg
        # Windows 7 Aero theme icons
        src/resources/icons/win7/checkbox-check.png
        src/resources/icons/win7/checkbox-check-disabled.png
        src/resources/icons/win7/radio-dot.png
        src/resources/icons/win7/radio-dot-disabled.png
        src/resources/icons/win7/arrow-up.svg
        src/resources/icons/win7/arrow-down.svg
        src/resources/icons/win7/arrow-left.svg
        src/resources/icons/win7/arrow-right.svg
        src/resources/icons/win7/dropdown-arrow.png
        src/resources/icons/win7/dropdown-arrow-disabled.png
        src/resources/icons/win7/spinbox-up.svg
        src/resources/icons/win7/spinbox-down.svg
        src/resources/icons/win7/right-arrow-small.png
        src/resources/icons/win7/down-right-arrow-small.png
        src/resources/icons/win7/sort-arrow-up.png
        src/resources/icons/win7/sort-arrow-down.png
        src/resources/icons/win7/close.png
        src/resources/icons/win7/tooltip-bg.png
        src/resources/icons/win7/scrollbar-grip-v.png
        src/resources/icons/win7/scrollbar-grip-h.png
        src/resources/icons/win7/slider-thumb.png
        src/resources/icons/win7/slider-thumb-hover.png
        src/resources/icons/win7/slider-thumb-pressed.png
        src/resources/icons/win7/slider-thumb-disabled.png
        src/resources/icons/win7/button-focus.png
)

qt_add_resources(WardPlanning "markers"
    PREFIX "/"
    BASE "src/resources"
    FILES
        src/resources/markers/marker_chapel.svg
        src/resources/markers/marker_home.svg
        src/resources/markers/marker_medical.svg
        src/resources/markers/marker_recovery.svg
        src/resources/markers/marker_special.svg
        src/resources/markers/marker_medical_recovery.svg
        src/resources/markers/marker_special_medical.svg
        src/resources/markers/marker_special_recovery.svg
        src/resources/markers/marker_special_medical_recovery.svg
        src/resources/markers/marker_antenna.svg
)

qt_add_resources(WardPlanning "styles"
    PREFIX "/"
    BASE "src/resources"
    FILES
        src/resources/styles/windows7.qss
)

# ============================================================================
# Platform-Specific Settings
# ============================================================================

# --- Windows ---
if(WIN32)
    set_target_properties(WardPlanning PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    # The installer's version, from the project's
    configure_file(packaging/windows/version.iss.in ${CMAKE_BINARY_DIR}/version.iss @ONLY)
endif()

# --- macOS ---
if(APPLE)
    set_target_properties(WardPlanning PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.church.wardplanning"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_NAME "Ward Planning"
    )
endif()

# --- Cross-platform definitions ---
target_compile_definitions(WardPlanning PRIVATE
    WARD_PLANNING_VERSION="${PROJECT_VERSION}"
)

# ============================================================================
# Tools
# ============================================================================
option(BUILD_TOOLS "Build development tools" ON)

if(BUILD_TOOLS)
    # PDF dump tool for analyzing MuPDF extraction
    add_executable(pdf_dump tools/pdf_dump.cpp)
    target_link_libraries(pdf_dump PRIVATE WardPlanningLib)

    # Test import tool for WardDirectoryImportService
    add_executable(test_import tools/test_import.cpp)
    target_link_libraries(test_import PRIVATE WardPlanningLib)
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
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "=== Ward Planning Configuration ===")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt Version: ${Qt6_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "")
