cmake_minimum_required(VERSION 3.21)

project(WardPlanning
    VERSION 1.1.0
    DESCRIPTION "Ward Planning - Ward Family, Ministering, and Emergency Response Planning"
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
    PrintSupport
    Test
)

# ============================================================================
# MuPDF for PDF text extraction (from vcpkg)
# ============================================================================
# The config package carries per-configuration library locations and pulls in
# freetype, harfbuzz, JPEG, OpenJPEG, gumbo and zlib as transitive dependencies,
# so a Debug build links the Debug libraries and a Release build the Release ones.
find_package(unofficial-libmupdf CONFIG REQUIRED)

# ============================================================================
# libsodium for document encryption and signing (from vcpkg)
# ============================================================================
find_package(unofficial-sodium CONFIG REQUIRED)

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
    src/models/SealingSet.cpp
    src/models/MinisteringGroup.cpp
    src/models/MinisteringDistrict.cpp
    src/models/EmergencyAsset.cpp
    src/models/DocumentChange.cpp
    src/models/EmergencyResponse.cpp
    src/models/StrictJson.cpp
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
    src/models/Person.h
    src/models/Family.h
    src/models/Team.h
    src/models/Tag.h
    src/models/Ward.h
    src/models/Stake.h
    src/models/DocumentMetadata.h
    src/models/Document.h
    src/models/SealingSet.h
    src/models/MinisteringGroup.h
    src/models/MinisteringDistrict.h
    src/models/EmergencyAsset.h
    src/models/ResponseArea.h
    src/models/DocumentChange.h
    src/models/Id.h
    src/models/EmergencyResponse.h
    src/models/StrictJson.h
)

set(COMMAND_SOURCES
    src/commands/CommandHistory.cpp
    src/commands/FamilyCommands.cpp
    src/commands/ImportWardDirectoryCommand.cpp
    src/commands/ImportMinisteringCommand.cpp
    src/commands/TeamCommands.cpp
    src/commands/TagCommands.cpp
    src/commands/EmergencyAssetCommands.cpp
    # src/commands/ImportCommands.cpp
)

set(COMMAND_HEADERS
    src/commands/Command.h
    src/commands/CommandHistory.h
    src/commands/FamilyCommands.h
    src/commands/ImportWardDirectoryCommand.h
    src/commands/ImportMinisteringCommand.h
    src/commands/TeamCommands.h
    src/commands/TagCommands.h
    src/commands/EmergencyAssetCommands.h
    # src/commands/ImportCommands.h
)

set(SERVICE_SOURCES
    src/services/DocumentManager.cpp
    src/services/PdfExtractor.cpp
    src/services/PdfFieldUtils.cpp
    src/services/PersonMatching.cpp
    src/services/WardDirectoryImportService.cpp
    src/services/WardDirectoryPdfParser.cpp
    src/services/MinisteringPdfParser.cpp
    src/services/MinisteringImportService.cpp
    src/services/TileDiskCacheService.cpp
    src/services/TileFetchService.cpp
    src/services/TileService.cpp
    src/services/UnitLookupService.cpp
    src/services/GeocodingService.cpp
    src/services/BackgroundGeocodingService.cpp
    src/services/GeoLocation.cpp
    src/services/EmergencyManager.cpp
    src/services/ReportGenerator.cpp
    src/services/CanonicalBytes.cpp
    src/services/CryptoTypes.cpp
    src/services/DocumentCrypto.cpp
    src/services/DocumentHeader.cpp
    src/services/HexCodec.cpp
    src/services/Reconcile.cpp
    src/services/ResponseMerge.cpp
    src/services/ServerContract.cpp
    src/services/DeviceIdentity.cpp
    src/services/DocumentFile.cpp
    src/services/DocumentRecords.cpp
    src/services/DocumentSession.cpp
    src/services/ServerConnection.cpp
    src/services/ServerVersion.cpp
    src/services/SessionTimers.cpp
    src/services/AppSettings.cpp
    src/services/SingleInstance.cpp
    src/services/IdentityFile.cpp
    src/services/IdentityService.cpp
    src/services/DocumentLibrary.cpp
    src/services/FileOpenRouter.cpp
    src/services/HttpServerApi.cpp
    src/services/HttpWire.cpp
    src/services/UpdateChecker.cpp
    # src/services/StatisticsService.cpp
    # src/services/ResourceService.cpp
    # src/services/SearchService.cpp
    # src/services/PreferencesService.cpp
    # src/services/PdfReportService.cpp
)

set(SERVICE_HEADERS
    src/services/DocumentManager.h
    src/services/PdfExtractor.h
    src/services/PersonMatching.h
    src/services/WardDirectoryImportService.h
    src/services/WardDirectoryPdfParser.h
    src/services/MinisteringPdfParser.h
    src/services/MinisteringImportService.h
    src/services/TileDiskCacheService.h
    src/services/TileFetchService.h
    src/services/TileTypes.h
    src/services/TileService.h
    src/services/UnitLookupService.h
    src/services/GeocodingService.h
    src/services/BackgroundGeocodingService.h
    src/services/GeoLocation.h
    src/services/EmergencyManager.h
    src/services/ReportGenerator.h
    src/services/CanonicalBytes.h
    src/services/CryptoTypes.h
    src/services/DocumentConstants.h
    src/services/DocumentCrypto.h
    src/services/DocumentHeader.h
    src/services/HexCodec.h
    src/services/Reconcile.h
    src/services/ResponseMerge.h
    src/services/ServerContract.h
    src/services/DeviceIdentity.h
    src/services/DocumentFile.h
    src/services/DocumentRecords.h
    src/services/DocumentSession.h
    src/services/ServerConnection.h
    src/services/ServerVersion.h
    src/services/SessionTimers.h
    src/services/AppSettings.h
    src/services/SingleInstance.h
    src/services/IdentityFile.h
    src/services/IdentityService.h
    src/services/DocumentLibrary.h
    src/services/FileOpenRouter.h
    src/services/HttpServerApi.h
    src/services/HttpWire.h
    src/services/UpdateChecker.h
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
    src/widgets/ContactAttemptDialog.cpp
    src/widgets/NotifyDialog.cpp
    src/widgets/FirstRunDialog.cpp
    src/widgets/ConflictDialog.cpp
    src/widgets/AccessDialog.cpp
    src/widgets/WardNotices.cpp
    src/widgets/SyncStatusWidget.cpp
    src/widgets/OpenWardDialog.cpp
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
    src/widgets/WardListDialog.cpp
    src/widgets/WardListView.cpp
    src/widgets/shared/FilterBar.cpp
    src/widgets/shared/FlowLayout.cpp
    src/widgets/shared/FilterChip.cpp
    src/widgets/shared/SearchField.cpp
    src/widgets/NeedsSubView.cpp
    src/widgets/SelectionPreservingTreeView.cpp
    src/widgets/SculptedItemDelegate.cpp
    src/widgets/EmergencyAssetView.cpp
    src/widgets/EmergencyBanner.cpp
    src/widgets/EmergencyProgressBar.cpp
    src/widgets/SidebarWidget.cpp
    src/widgets/MinisteringTreeView.cpp
    src/widgets/UnassignedTreeView.cpp
    src/widgets/MinisteringTabView.cpp
    # src/widgets/Sidebar.cpp
    # src/widgets/MinisteringSidebar.cpp
    # src/widgets/ResourceView.cpp
    # src/widgets/TagsView.cpp
    src/widgets/TeamsView.cpp
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
    src/widgets/ContactAttemptDialog.h
    src/widgets/NotifyDialog.h
    src/widgets/FirstRunDialog.h
    src/widgets/ConflictDialog.h
    src/widgets/AccessDialog.h
    src/widgets/WardNotices.h
    src/widgets/SyncStatusWidget.h
    src/widgets/OpenWardDialog.h
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
    src/widgets/WardListDialog.h
    src/widgets/WardListView.h
    src/widgets/shared/FilterBar.h
    src/widgets/shared/FlowLayout.h
    src/widgets/shared/FilterChip.h
    src/widgets/shared/SearchField.h
    src/widgets/NeedsSubView.h
    src/widgets/SelectionPreservingTreeView.h
    src/widgets/SculptedItemDelegate.h
    src/widgets/EmergencyAssetView.h
    src/widgets/EmergencyBanner.h
    src/widgets/EmergencyProgressBar.h
    src/widgets/SidebarWidget.h
    # src/widgets/Sidebar.h
    # src/widgets/MinisteringSidebar.h
    # src/widgets/ResourceView.h
    # src/widgets/TagsView.h
    src/widgets/TeamsView.h
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
    src/listmodels/TeamListModel.cpp
    src/listmodels/TagListModel.cpp
    src/listmodels/TaskListModel.cpp
    src/listmodels/TeamsTreeModel.cpp
    # src/listmodels/DistrictTreeModel.cpp
    # src/listmodels/ResourceTreeModel.cpp
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
    src/listmodels/TeamListModel.h
    src/listmodels/TagListModel.h
    src/listmodels/TeamsTreeModel.h
    # src/listmodels/DistrictTreeModel.h
    # src/listmodels/ResourceTreeModel.h
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

# Mark third-party includes as SYSTEM to suppress warnings from their headers.
# MuPDF's own include directory arrives through its imported target, which CMake
# already treats as SYSTEM.
target_include_directories(WardPlanningLib SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty
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
    unofficial::libmupdf::libmupdf
    unofficial-sodium::sodium
)

target_compile_definitions(WardPlanningLib PUBLIC HAVE_MUPDF)

# project(VERSION ...) is the one place the version is written; main.cpp hands it
# to QCoreApplication, and everything user-visible reads it back from there.
target_compile_definitions(WardPlanningLib PUBLIC
    WARD_PLANNING_VERSION="${PROJECT_VERSION}"
)


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

    # Windows application icon
    # target_sources(WardPlanning PRIVATE packaging/windows/WardPlanning.rc)

    # Windows-specific: Include skeuomorphic stylesheet in resources
    # The stylesheet is loaded at runtime on Windows only
    message(STATUS "Windows build: Skeuomorphic styling enabled")
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

    # macOS-specific: Use native styling (no custom stylesheet)
    # Qt automatically uses native Aqua controls
    message(STATUS "macOS build: Native Aqua styling enabled")

    # Enable Retina/HiDPI support
    set_target_properties(WardPlanning PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "WardPlanning.icns"
    )

    # Copy icon to bundle
    # set(MACOS_ICON ${CMAKE_SOURCE_DIR}/packaging/macos/WardPlanning.icns)
    # set_source_files_properties(${MACOS_ICON} PROPERTIES
    #     MACOSX_PACKAGE_LOCATION "Resources"
    # )
    # target_sources(WardPlanning PRIVATE ${MACOS_ICON})
endif()

# --- Cross-platform definitions ---
target_compile_definitions(WardPlanning PRIVATE
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
# Installation
# ============================================================================
install(TARGETS WardPlanning
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install Qt runtime (Windows)
if(WIN32)
    # qt_generate_deploy_app_script(
    #     TARGET WardPlanning
    #     OUTPUT_SCRIPT deploy_script
    #     NO_UNSUPPORTED_PLATFORM_ERROR
    # )
    # install(SCRIPT ${deploy_script})
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
