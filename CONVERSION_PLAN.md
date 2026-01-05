# Emergency Plan: Flutter to C++ Qt Conversion Plan

## Executive Summary

This document outlines the comprehensive plan for converting the Emergency Plan application from Flutter/Dart to C++/Qt. The original application is a desktop ward/stake management system with ~40,000 lines of Dart code featuring interactive mapping, emergency response planning, and resource management.

---

## Table of Contents

1. [Technology Stack Selection](#1-technology-stack-selection)
2. [Architecture Overview](#2-architecture-overview)
3. [Project Structure](#3-project-structure)
4. [Data Models Conversion](#4-data-models-conversion)
5. [State Management Strategy](#5-state-management-strategy)
6. [Command Pattern Implementation](#6-command-pattern-implementation)
7. [Services Layer Conversion](#7-services-layer-conversion)
8. [UI Components Mapping](#8-ui-components-mapping)
9. [Map Integration](#9-map-integration)
10. [PDF Handling](#10-pdf-handling)
11. [File I/O and Persistence](#11-file-io-and-persistence)
12. [Third-Party Libraries](#12-third-party-libraries)
13. [Build System Configuration](#13-build-system-configuration)
14. [Testing Strategy](#14-testing-strategy)
15. [Migration Phases](#15-migration-phases)
16. [Risk Assessment](#16-risk-assessment)
17. [Appendix: File-by-File Mapping](#17-appendix-file-by-file-mapping)

---

## 1. Technology Stack Selection

### Target Stack

| Component | Technology | Rationale |
|-----------|------------|-----------|
| **Language** | C++17/20 | Modern C++ features, smart pointers, optional types |
| **UI Framework** | Qt 6.x (Qt Widgets primary) | Mature, cross-platform, excellent documentation |
| **UI Style** | Windows 7 / Skeuomorphic | Classic tactile design with depth and dimension |
| **Build System** | CMake 3.21+ | Industry standard, Qt integration |
| **JSON** | Qt JSON (QJsonDocument) | Built-in, no external dependency |
| **HTTP** | Qt Network (QNetworkAccessManager) | Built-in networking |
| **PDF** | QPdfDocument (reading) + custom/libharu (generation) | Qt 6 native PDF support |
| **Mapping** | Qt Widgets with custom tile rendering | Classic look, offline support |
| **Testing** | Qt Test + Google Test | Comprehensive testing framework |
| **Package Manager** | vcpkg or Conan | Dependency management |

### Visual Design Philosophy

This application uses **platform-native styling** with customization:

| Platform | Style | Approach |
|----------|-------|----------|
| **Windows** | Windows 7 / Skeuomorphic | Vista style + custom QSS with gradients, bevels, depth |
| **macOS** | Native Aqua | macOS style, no custom stylesheet, native look and feel |

**Windows Design Characteristics:**
- Depth and dimension: Gradient buttons, beveled edges, subtle shadows
- Visual richness: Detailed icons with shading and perspective
- Tactile affordances: Buttons look pressable, inputs look like input fields
- Classic Windows 7 styling

**macOS Design Characteristics:**
- Native Aqua controls and window chrome
- System font (San Francisco)
- Native scrollbars, buttons, and dialogs
- Follows Apple Human Interface Guidelines

See **DESIGN_GUIDE.md** for Windows-specific visual specifications.

```cpp
// Platform-specific styling
#ifdef Q_OS_WIN
    app.setStyle(QStyleFactory::create("windowsvista"));

    // Load skeuomorphic stylesheet on Windows only
    QFile styleFile(":/styles/skeuomorphic.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        app.setStyleSheet(styleFile.readAll());
    }
#elif defined(Q_OS_MACOS)
    // Use native macOS style - no custom stylesheet
    app.setStyle(QStyleFactory::create("macos"));
#endif
```

### Cross-Platform Development Strategy

Both Windows and macOS versions will be developed simultaneously:

1. **Shared codebase**: All models, commands, services, and viewmodels are platform-agnostic
2. **Platform-specific styling**: Only the visual appearance differs
3. **Conditional compilation**: Use `#ifdef Q_OS_WIN` / `#ifdef Q_OS_MACOS` where needed
4. **CI/CD builds**: Build and test on both platforms in parallel
5. **Layout considerations**: Use layout managers that adapt to platform font/widget sizes

### Qt Module Requirements

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Quick
    QuickWidgets
    Network
    Sql
    Svg
    Pdf
    Positioning
    Location  # For mapping
    Concurrent
    Test
)
```

---

## 2. Architecture Overview

### Flutter vs Qt Architecture Mapping

| Flutter Concept | Qt Equivalent |
|-----------------|---------------|
| Riverpod Providers | Q_PROPERTY + signals/slots + QAbstractItemModel |
| StatelessWidget | QWidget or QML Component |
| StatefulWidget | QWidget with member state |
| ChangeNotifier | QObject with signals |
| StreamBuilder | Signal/slot connections |
| Navigator | QStackedWidget or QML StackView |
| Theme | QPalette + QStyle |
| BuildContext | Parent QWidget pointer |

### Layered Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        UI Layer                              │
│   (Qt Widgets / QML Components)                              │
├─────────────────────────────────────────────────────────────┤
│                    ViewModel Layer                           │
│   (QObject-based controllers with Q_PROPERTY)                │
├─────────────────────────────────────────────────────────────┤
│                    Service Layer                             │
│   (Stateless utility classes)                                │
├─────────────────────────────────────────────────────────────┤
│                    Command Layer                             │
│   (Command pattern for undo/redo)                            │
├─────────────────────────────────────────────────────────────┤
│                    Model Layer                               │
│   (Data structures + QAbstractItemModel wrappers)            │
├─────────────────────────────────────────────────────────────┤
│                  Persistence Layer                           │
│   (JSON serialization, file I/O)                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Project Structure

```
WardPlanningQt/
├── CMakeLists.txt                    # Root CMake configuration
├── cmake/
│   ├── CompilerWarnings.cmake        # Compiler settings
│   ├── Dependencies.cmake            # External dependencies
│   └── QtHelpers.cmake               # Qt-specific helpers
│
├── src/
│   ├── main.cpp                      # Application entry point
│   ├── app/
│   │   ├── Application.h/cpp         # QApplication subclass
│   │   └── MainWindow.h/cpp          # Main window with menus
│   │
│   ├── models/                       # Data models
│   │   ├── Document.h/cpp            # Root state container
│   │   ├── Family.h/cpp           # Family entity
│   │   ├── Person.h/cpp              # Person entity
│   │   ├── Team.h/cpp                # Emergency team
│   │   ├── Tag.h/cpp                 # Tag entity
│   │   ├── MinisteringVisit.h/cpp    # Visit tracking
│   │   ├── EQMinisteringGroup.h/cpp  # EQ group
│   │   ├── RSMinisteringGroup.h/cpp  # RS group
│   │   ├── MinisteringDistrict.h/cpp # District grouping
│   │   ├── ResourceType.h/cpp        # Skills/equipment
│   │   ├── ResourceCategory.h/cpp    # Category grouping
│   │   ├── MarkerDecorationType.h    # Marker enum
│   │   ├── ViewMode.h                # View mode enum
│   │   └── Gender.h                  # Gender enum
│   │
│   ├── models/adapters/              # Qt model adapters
│   │   ├── FamilyListModel.h/cpp  # QAbstractListModel
│   │   ├── PersonListModel.h/cpp     # QAbstractListModel
│   │   ├── TeamListModel.h/cpp       # QAbstractListModel
│   │   ├── TagListModel.h/cpp        # QAbstractListModel
│   │   ├── DistrictTreeModel.h/cpp   # QAbstractItemModel (tree)
│   │   └── ResourceTreeModel.h/cpp   # QAbstractItemModel (tree)
│   │
│   ├── commands/                     # Command pattern
│   │   ├── Command.h                 # Base interface
│   │   ├── CommandHistory.h/cpp      # Undo/redo stack
│   │   ├── FamilyCommands.h/cpp   # Family CRUD
│   │   ├── TeamCommands.h/cpp        # Team management
│   │   ├── MinisteringCommands.h/cpp # EQ/RS operations
│   │   ├── TagCommands.h/cpp         # Tag operations
│   │   ├── ResourceCommands.h/cpp    # Resource operations
│   │   └── ImportCommands.h/cpp      # Bulk import
│   │
│   ├── services/                     # Business logic
│   │   ├── DocumentManager.h/cpp     # Central state manager
│   │   ├── JsonService.h/cpp         # JSON import/export
│   │   ├── PdfImportService.h/cpp    # PDF parsing
│   │   ├── PdfReportService.h/cpp    # PDF generation
│   │   ├── FilterService.h/cpp       # Filtering logic
│   │   ├── ResourceService.h/cpp     # Resource management
│   │   ├── StatisticsService.h/cpp   # Stats calculations
│   │   ├── GeocodingService.h/cpp    # Address geocoding
│   │   ├── TileCacheService.h/cpp    # Map tile caching
│   │   ├── PreferencesService.h/cpp  # Settings persistence
│   │   ├── SearchService.h/cpp       # Full-text search
│   │   └── RfLosService.h/cpp        # RF calculations
│   │
│   ├── viewmodels/                   # View models (controllers)
│   │   ├── DocumentViewModel.h/cpp   # Main document state
│   │   ├── FamilyViewModel.h/cpp  # Family editing
│   │   ├── MapViewModel.h/cpp        # Map state
│   │   ├── MinisteringViewModel.h/cpp # EQ/RS state
│   │   ├── TeamViewModel.h/cpp       # Team management
│   │   ├── TagViewModel.h/cpp        # Tag management
│   │   ├── ResourceViewModel.h/cpp   # Resource state
│   │   └── FilterViewModel.h/cpp     # Filter state
│   │
│   ├── widgets/                      # Qt Widgets UI
│   │   ├── MainWindow.h/cpp          # Main application window
│   │   ├── Sidebar.h/cpp             # Navigation sidebar
│   │   ├── WardListView.h/cpp        # Family list
│   │   ├── MapWidget.h/cpp           # Map container
│   │   ├── MinisteringSidebar.h/cpp  # EQ/RS content
│   │   ├── EmergencyView.h/cpp       # Emergency response
│   │   ├── ResourceView.h/cpp        # Skills/resources
│   │   ├── TagsView.h/cpp            # Tag management
│   │   ├── TeamsView.h/cpp           # Team management
│   │   │
│   │   ├── dialogs/                  # Dialog windows
│   │   │   ├── FamilyDialog.h/cpp # Family editing
│   │   │   ├── PersonForm.h/cpp      # Person editing
│   │   │   ├── VisitDialog.h/cpp     # Visit recording
│   │   │   ├── ImportDialog.h/cpp    # PDF import
│   │   │   ├── StatisticsDialog.h/cpp# Statistics
│   │   │   ├── TagDialog.h/cpp       # Tag editing
│   │   │   └── ResourceDialog.h/cpp  # Resource creation
│   │   │
│   │   └── shared/                   # Reusable widgets
│   │       ├── SearchField.h/cpp     # Search input
│   │       ├── FilterChips.h/cpp     # Chip-based filters
│   │       ├── FamilyTile.h/cpp   # Family display
│   │       ├── PersonTile.h/cpp      # Person display
│   │       └── BadgeWidget.h/cpp     # Resource badges
│   │
│   ├── qml/                          # QML components
│   │   ├── MapView.qml               # Interactive map
│   │   ├── MapMarker.qml             # Marker component
│   │   ├── controls/                 # Custom QML controls
│   │   │   ├── SearchBar.qml
│   │   │   └── FilterPanel.qml
│   │   └── qmldir                    # QML module definition
│   │
│   └── resources/                    # Resource files
│       ├── resources.qrc             # Qt resource file
│       ├── markers/                  # SVG marker icons
│       │   ├── marker_home.svg
│       │   ├── marker_medical.svg
│       │   └── ...
│       ├── config/
│       │   └── resource_types.txt    # Default resources
│       └── icons/                    # Application icons
│
├── tests/
│   ├── CMakeLists.txt
│   ├── models/
│   │   ├── DocumentTest.cpp
│   │   ├── FamilyTest.cpp
│   │   └── ...
│   ├── commands/
│   │   ├── FamilyCommandsTest.cpp
│   │   └── ...
│   └── services/
│       ├── JsonServiceTest.cpp
│       └── ...
│
├── docs/
│   ├── ARCHITECTURE.md               # Qt architecture docs
│   ├── BUILDING.md                   # Build instructions
│   └── API.md                        # API documentation
│
└── packaging/
    ├── windows/
    │   ├── installer.nsi             # NSIS installer
    │   └── WardPlanning.rc           # Windows resources
    ├── macos/
    │   └── Info.plist
    └── linux/
        └── wardplanning.desktop
```

---

## 4. Data Models Conversion

### General Conversion Pattern

**Flutter (Dart) Immutable Model:**
```dart
class Person {
  final String id;
  final String name;
  final String? phone;
  final Gender? gender;

  const Person({
    required this.id,
    required this.name,
    this.phone,
    this.gender,
  });

  Person copyWith({...}) => Person(...);

  Map<String, dynamic> toJson() => {...};
  factory Person.fromJson(Map<String, dynamic> json) => ...;
}
```

**C++ Qt Equivalent:**
```cpp
// Person.h
#pragma once

#include <QString>
#include <QJsonObject>
#include <optional>
#include "Gender.h"

class Person {
public:
    Person() = default;
    Person(QString id, QString name,
           std::optional<QString> phone = std::nullopt,
           std::optional<Gender> gender = std::nullopt);

    // Getters
    QString id() const { return m_id; }
    QString name() const { return m_name; }
    std::optional<QString> phone() const { return m_phone; }
    std::optional<Gender> gender() const { return m_gender; }

    // Immutable update (returns new instance)
    Person withName(const QString& name) const;
    Person withPhone(const std::optional<QString>& phone) const;
    Person withGender(const std::optional<Gender>& gender) const;

    // Serialization
    QJsonObject toJson() const;
    static Person fromJson(const QJsonObject& json);

    // Equality
    bool operator==(const Person& other) const;
    bool operator!=(const Person& other) const;

private:
    QString m_id;
    QString m_name;
    std::optional<QString> m_phone;
    std::optional<Gender> m_gender;
};
```

### Complete Model Mapping

| Flutter Model | C++ Class | Key Differences |
|---------------|-----------|-----------------|
| `Document` | `Document` | Uses std::vector instead of List, Q_PROPERTY for Qt integration |
| `Family` | `Family` | Members as std::vector<Person> |
| `Person` | `Person` | Optional fields use std::optional |
| `Team` | `Team` | Color stored as QColor |
| `Tag` | `Tag` | Sets for personIds/familyIds (faster lookup) |
| `MinisteringVisit` | `MinisteringVisit` | QDateTime instead of DateTime |
| `EQMinisteringGroup` | `EQMinisteringGroup` | Inherits from MinisteringGroupBase |
| `RSMinisteringGroup` | `RSMinisteringGroup` | Inherits from MinisteringGroupBase |
| `MinisteringDistrict` | `MinisteringDistrict` | Shared by EQ/RS |
| `ResourceType` | `ResourceType` | Config parsing in service |
| `ResourceCategory` | `ResourceCategory` | Enum for level |
| `MarkerDecorationType` | `MarkerDecorationType` | C++ enum class |
| `ViewMode` | `ViewMode` | C++ enum class |
| `Gender` | `Gender` | C++ enum class |

### Enum Conversions

```cpp
// Gender.h
enum class Gender {
    Male,
    Female
};

QString genderToString(Gender g);
Gender genderFromString(const QString& s);

// ViewMode.h
enum class ViewMode {
    EldersQuorum,
    ReliefSociety,
    EmergencyResponse,
    Tags
};

// MarkerDecorationType.h
enum class MarkerDecorationType {
    None,
    Medical,    // MED - Blue
    Recovery,   // REC - Green
    Communication, // COM - Antenna
    SpecialNeeds   // SPEC - Red
};
```

---

## 5. State Management Strategy

### Flutter Riverpod → Qt Signals/Slots + Q_PROPERTY

**Flutter Provider Pattern:**
```dart
final documentProvider = StateProvider<Document>((ref) => Document.empty());

final familiesProvider = Provider<List<Family>>((ref) {
  return ref.watch(documentProvider).families;
});

final filteredFamiliesProvider = Provider<List<Family>>((ref) {
  final families = ref.watch(familiesProvider);
  final filters = ref.watch(filtersProvider);
  return FilterService.filter(families, filters);
});
```

**Qt Equivalent:**

```cpp
// DocumentViewModel.h
class DocumentViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(Document* document READ document NOTIFY documentChanged)
    Q_PROPERTY(QList<Family*> families READ families NOTIFY familiesChanged)
    Q_PROPERTY(QList<Family*> filteredFamilies READ filteredFamilies
               NOTIFY filteredFamiliesChanged)

public:
    explicit DocumentViewModel(QObject* parent = nullptr);

    Document* document() const { return m_document.get(); }
    QList<Family*> families() const;
    QList<Family*> filteredFamilies() const;

    // Commands
    void addFamily(const Family& family);
    void updateFamily(const Family& family);
    void deleteFamily(const QString& id);

    // Undo/Redo
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

signals:
    void documentChanged();
    void familiesChanged();
    void filteredFamiliesChanged();
    void canUndoChanged();
    void canRedoChanged();

private:
    std::unique_ptr<Document> m_document;
    std::unique_ptr<CommandHistory> m_commandHistory;
    FilterService m_filterService;

    void onDocumentModified();
};
```

### Qt Model/View for Lists

For list displays, use QAbstractItemModel:

```cpp
// FamilyListModel.h
class FamilyListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        AddressRole,
        LatitudeRole,
        LongitudeRole,
        MemberCountRole,
        HasSpecialNeedsRole,
        TeamColorRole
    };

    explicit FamilyListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFamilies(const std::vector<Family>& families);
    void updateFamily(const Family& family);

private:
    std::vector<Family> m_families;
};
```

---

## 6. Command Pattern Implementation

### Base Command Interface

```cpp
// Command.h
#pragma once

#include <QString>
#include <memory>
#include "Document.h"

class Command {
public:
    virtual ~Command() = default;

    // Execute command, return new document state
    virtual Document execute(const Document& document) = 0;

    // Undo command, return previous document state
    virtual Document undo(const Document& document) = 0;

    // Human-readable description
    virtual QString description() const = 0;
};

using CommandPtr = std::unique_ptr<Command>;
```

### Command History

```cpp
// CommandHistory.h
class CommandHistory : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)

public:
    explicit CommandHistory(QObject* parent = nullptr);

    // Execute a command and add to history
    Document execute(CommandPtr command, const Document& document);

    // Undo/Redo operations
    Document undo(const Document& document);
    Document redo(const Document& document);

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

    // Dirty state tracking
    void markSaved();
    bool isDirty() const;

signals:
    void canUndoChanged();
    void canRedoChanged();
    void dirtyChanged();

private:
    std::vector<CommandPtr> m_undoStack;
    std::vector<CommandPtr> m_redoStack;
    size_t m_savedPosition = 0;
};
```

### Example Command Implementation

```cpp
// FamilyCommands.h
class AddFamilyCommand : public Command {
public:
    explicit AddFamilyCommand(Family family);

    Document execute(const Document& document) override;
    Document undo(const Document& document) override;
    QString description() const override;

private:
    Family m_family;
};

// FamilyCommands.cpp
Document AddFamilyCommand::execute(const Document& document) {
    auto families = document.families();
    families.push_back(m_family);
    return document.withFamilies(std::move(families));
}

Document AddFamilyCommand::undo(const Document& document) {
    auto families = document.families();
    families.erase(
        std::remove_if(families.begin(), families.end(),
            [this](const Family& h) { return h.id() == m_family.id(); }),
        families.end()
    );
    return document.withFamilies(std::move(families));
}
```

---

## 7. Services Layer Conversion

### Service Mapping

| Flutter Service | Qt Service | Key Implementation Notes |
|-----------------|------------|--------------------------|
| `DocumentManager` | `DocumentManager` | Owns Document, CommandHistory; emits signals |
| `JsonService` | `JsonService` | Uses QJsonDocument |
| `PdfImportService` | `PdfImportService` | Uses QPdfDocument + custom parsing |
| `PdfReportService` | `PdfReportService` | Uses QPainter + QPdfWriter |
| `FilterService` | `FilterService` | Stateless filter functions |
| `ResourceService` | `ResourceService` | Resource config parsing |
| `StatisticsService` | `StatisticsService` | Pure computation |
| `GeocodingService` | `GeocodingService` | QNetworkAccessManager for HTTP |
| `OfflineTileService` | `TileCacheService` | QDir for file caching |
| `PreferencesService` | `PreferencesService` | QSettings |
| `SearchService` | `SearchService` | QString pattern matching |
| `RFLOSService` | `RfLosService` | Elevation calculations |
| `BackgroundGeocodingService` | `GeocodingWorker` | QThread + signals |

### Async Service Pattern

```cpp
// GeocodingService.h
class GeocodingService : public QObject {
    Q_OBJECT

public:
    explicit GeocodingService(QObject* parent = nullptr);

    // Async geocoding request
    void geocodeAddress(const QString& address, const QString& familyId);

signals:
    void geocodingComplete(QString familyId, double latitude, double longitude);
    void geocodingFailed(QString familyId, QString error);

private slots:
    void onNetworkReply(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_networkManager;
    QMap<QNetworkReply*, QString> m_pendingRequests;
};
```

### Background Worker Pattern

```cpp
// GeocodingWorker.h
class GeocodingWorker : public QObject {
    Q_OBJECT

public:
    explicit GeocodingWorker(QObject* parent = nullptr);

public slots:
    void processQueue(const QStringList& addresses);
    void cancel();

signals:
    void progress(int current, int total);
    void addressGeocoded(QString address, double lat, double lng);
    void finished();
    void error(QString message);

private:
    std::atomic<bool> m_cancelled{false};
};

// Usage in main thread:
QThread* thread = new QThread;
GeocodingWorker* worker = new GeocodingWorker;
worker->moveToThread(thread);

connect(thread, &QThread::finished, worker, &QObject::deleteLater);
connect(this, &MyClass::startGeocoding, worker, &GeocodingWorker::processQueue);
connect(worker, &GeocodingWorker::addressGeocoded, this, &MyClass::onAddressGeocoded);

thread->start();
```

---

## 8. UI Components Mapping

### Design Approach: Windows 7 / Skeuomorphic

The UI will follow a classic Windows 7 / skeuomorphic design philosophy:
- Use Qt Widgets exclusively (no QML) for consistent classic appearance
- Windows Vista style as base with custom QSS enhancements
- Gradient backgrounds, beveled buttons, 3D-style controls
- Detailed icons with shading and perspective
- See **DESIGN_GUIDE.md** for complete specifications

### Widget Hierarchy

| Flutter Widget | Qt Widget | Notes |
|----------------|-----------|-------|
| `MainScreen` | `MainWindow` | QMainWindow with classic menu bar |
| `Sidebar` | `QSplitter` + `QTabWidget` | Classic tabbed sidebar |
| `WardListView` | `QListView` + `FamilyListModel` | Alternating row colors, 3D selection |
| `MapView` | `QWidget` + custom painting | Classic tile-based map widget |
| `FamilyDialog` | `QDialog` | Classic form dialog with groupboxes |
| `PersonFormCard` | `QGroupBox` | Raised border, gradient header |
| `SearchField` | `QLineEdit` with icon | Classic text input styling |
| `FilterChips` | `QToolBar` + `QToolButton` | Toggle buttons with pressed states |

### Main Window Structure

```cpp
// MainWindow.h
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onUndo();
    void onRedo();
    void onViewModeChanged(ViewMode mode);

private:
    void setupMenuBar();
    void setupUI();
    void setupConnections();
    void updateTitle();
    bool maybeSave();

    // UI Components
    QSplitter* m_splitter;
    Sidebar* m_sidebar;
    QStackedWidget* m_contentStack;
    MapWidget* m_mapWidget;

    // View Models
    std::unique_ptr<DocumentViewModel> m_documentViewModel;
    std::unique_ptr<FilterViewModel> m_filterViewModel;

    // Current file
    QString m_currentFilePath;
};
```

### QML Map Integration

```qml
// MapView.qml
import QtQuick 2.15
import QtLocation 5.15
import QtPositioning 5.15

Item {
    id: root

    property var families: []
    property var highlightedIds: []
    property string selectedId: ""

    signal familyClicked(string familyId)
    signal mapClicked(real latitude, real longitude)

    Plugin {
        id: mapPlugin
        name: "osm"  // or "maplibregl" for vector tiles
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin
        center: QtPositioning.coordinate(40.0, -111.0)
        zoomLevel: 12

        MapItemView {
            model: familiesModel
            delegate: MapQuickItem {
                coordinate: QtPositioning.coordinate(model.latitude, model.longitude)
                anchorPoint.x: markerImage.width / 2
                anchorPoint.y: markerImage.height

                sourceItem: Image {
                    id: markerImage
                    source: getMarkerSource(model)
                    width: 32
                    height: 40
                    opacity: isHighlighted(model.id) ? 1.0 : 0.6

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.familyClicked(model.id)
                    }
                }
            }
        }
    }

    function getMarkerSource(family) {
        // Return appropriate marker SVG based on decorations
    }

    function isHighlighted(id) {
        return highlightedIds.indexOf(id) !== -1
    }

    function centerOn(lat, lng, zoom) {
        map.center = QtPositioning.coordinate(lat, lng)
        if (zoom > 0) map.zoomLevel = zoom
    }
}
```

### Dialog Implementation

```cpp
// FamilyDialog.h
class FamilyDialog : public QDialog {
    Q_OBJECT

public:
    explicit FamilyDialog(QWidget* parent = nullptr);
    explicit FamilyDialog(const Family& family, QWidget* parent = nullptr);

    Family family() const;

private slots:
    void onAddPerson();
    void onRemovePerson(int index);
    void onGeocodeAddress();

private:
    void setupUI();
    void populateFromFamily(const Family& family);

    // Form fields
    QLineEdit* m_nameEdit;
    QLineEdit* m_addressEdit;
    QDoubleSpinBox* m_latitudeSpinBox;
    QDoubleSpinBox* m_longitudeSpinBox;
    QListWidget* m_membersListWidget;

    // Original data
    QString m_familyId;
    std::vector<Person> m_members;
};
```

---

## 9. Map Integration

### Strategy Options

**Option A: Qt Location with OSM Plugin (Recommended for simplicity)**
- Built into Qt 6
- OpenStreetMap tiles
- Familiar QtQuick API
- Limited offline support

**Option B: MapLibre GL Native**
- Vector tiles (better quality at all zoom levels)
- Full offline support
- More complex integration
- Better performance

**Option C: Custom Tile Renderer**
- Full control over rendering
- Most work to implement
- Best offline support
- Matches Flutter implementation closely

### Recommended: Qt Location + Custom Tile Cache

```cpp
// TileCacheService.h
class TileCacheService : public QObject {
    Q_OBJECT

public:
    explicit TileCacheService(QObject* parent = nullptr);

    // Check if tile is cached
    bool hasTile(int z, int x, int y, const QString& layer);

    // Get cached tile path (or download if missing)
    QString getTilePath(int z, int x, int y, const QString& layer);

    // Pre-cache region
    void cacheRegion(const QRectF& bounds, int minZoom, int maxZoom);

signals:
    void cacheProgress(int current, int total);
    void cacheComplete();

private:
    QString m_cacheDir;
    QNetworkAccessManager* m_networkManager;
};
```

### Marker Rendering

```cpp
// MarkerRenderer.h
class MarkerRenderer {
public:
    static QPixmap renderMarker(const Family& family,
                                const std::vector<ResourceCategory>& categories,
                                bool isHighlighted);

private:
    static QPixmap loadSvg(const QString& path);
    static QPixmap compositeMarkers(const std::vector<MarkerDecorationType>& decorations);

    static QCache<QString, QPixmap> s_cache;
};
```

---

## 10. PDF Handling

### PDF Import Architecture

The PDF import system is split into layers:

1. **Low-level**: `PdfExtractor` - MuPDF wrapper for text/position extraction
2. **Parsers**: `WardDirectoryPdfParser`, `MinisteringPdfParser` - Format-specific parsing
3. **Services**: `WardDirectoryImportService`, `MinisteringImportService` - Orchestration and family matching
4. **Commands**: `ImportWardDirectoryCommand`, `ImportEQMinisteringCommand`, `ImportRSMinisteringCommand`

```cpp
// WardDirectoryImportService.h
struct WardDirectoryImportResult {
    bool success = false;
    QHash<QString, Family> families;  // Complete merged set
    QStringList errors;
    QString wardName;
    QString wardUnitNumber;
};

class WardDirectoryImportService : public QObject {
    WardDirectoryImportResult importFromPdf(
        const QString& pdfPath,
        const QList<Family>& existingFamilies = {});
};

// MinisteringImportService.h
struct MinisteringImportResult {
    bool success = false;
    bool isRSFormat = false;
    QHash<QString, MinisteringDistrict> districts;
    QHash<QString, EQMinisteringGroup> eqGroups;
    QHash<QString, RSMinisteringGroup> rsGroups;
    QHash<QString, Family> families;  // Complete merged set
    QList<Family> newFamilies;
    QList<Family> updatedFamilies;
    // ...
};

class MinisteringImportService : public QObject {
    MinisteringImportResult importFromPdf(
        const QString& pdfPath,
        const QList<Family>& existingFamilies);
};
```

### Import Flow

Both imports follow the same pattern:
1. Parse PDF → get families with temp IDs
2. Match against existing families by surname
3. Build merged families hash (existing + updated + new)
4. Command calls `document.setFamilies()` with merged hash

This ensures:
- Importing into empty document works correctly
- Importing when families exist matches and preserves IDs
- Undo restores previous state completely

### PDF Generation (Reports)

```cpp
// PdfReportService.h
class PdfReportService : public QObject {
    Q_OBJECT

public:
    explicit PdfReportService(QObject* parent = nullptr);

    // Generate emergency resource report
    void generateResourceReport(const QString& filePath,
                               const Document& document,
                               const std::vector<QString>& teamIds);

    // Generate tag report
    void generateTagReport(const QString& filePath,
                          const Document& document,
                          const std::vector<QString>& tagIds);

private:
    void setupPage(QPainter& painter, const QPageLayout& layout);
    void drawHeader(QPainter& painter, const QString& title);
    void drawFamilySection(QPainter& painter, const std::vector<Family>& families);
    void drawPersonSection(QPainter& painter, const std::vector<Person>& persons);
};
```

---

## 11. File I/O and Persistence

### JSON Serialization

```cpp
// JsonService.h
class JsonService {
public:
    static constexpr int CURRENT_VERSION = 3;

    // Save document to file
    static bool saveDocument(const QString& filePath, const Document& document);

    // Load document from file
    static std::optional<Document> loadDocument(const QString& filePath);

    // Export to JSON string
    static QByteArray toJson(const Document& document);

    // Parse from JSON string
    static std::optional<Document> fromJson(const QByteArray& json);

private:
    static Document migrate(const QJsonObject& json, int fromVersion);
};
```

### Settings Persistence

```cpp
// PreferencesService.h
class PreferencesService : public QObject {
    Q_OBJECT
    Q_PROPERTY(ViewMode lastViewMode READ lastViewMode WRITE setLastViewMode)
    Q_PROPERTY(QString lastFilePath READ lastFilePath WRITE setLastFilePath)
    Q_PROPERTY(QByteArray windowGeometry READ windowGeometry WRITE setWindowGeometry)

public:
    explicit PreferencesService(QObject* parent = nullptr);

    ViewMode lastViewMode() const;
    void setLastViewMode(ViewMode mode);

    QString lastFilePath() const;
    void setLastFilePath(const QString& path);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);

private:
    QSettings m_settings;
};
```

---

## 12. Third-Party Libraries

### Required Dependencies

| Library | Purpose | Source |
|---------|---------|--------|
| Qt 6.5+ | Core framework | qt.io |
| libharu (optional) | Advanced PDF generation | github.com/libharu/libharu |
| GoogleTest | Unit testing | github.com/google/googletest |

### vcpkg Integration

```cmake
# CMakePresets.json
{
    "version": 3,
    "configurePresets": [
        {
            "name": "default",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build",
            "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        }
    ]
}
```

```
# vcpkg.json
{
    "name": "wardplanning",
    "version": "1.0.0",
    "dependencies": [
        "gtest"
    ]
}
```

---

## 13. Build System Configuration

### Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.21)
project(WardPlanning VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets Quick QuickWidgets
    Network Svg Pdf Positioning Location
    Concurrent Test
)

# Include subdirectories
add_subdirectory(src)
add_subdirectory(tests)

# Main executable
qt_add_executable(WardPlanning
    src/main.cpp
)

target_link_libraries(WardPlanning PRIVATE
    WardPlanningLib
    Qt6::Widgets
    Qt6::Quick
)

# Resources
qt_add_resources(WardPlanning "resources"
    PREFIX "/"
    FILES
        src/resources/markers/marker_home.svg
        # ... more resources
)

# QML module
qt_add_qml_module(WardPlanning
    URI WardPlanning
    VERSION 1.0
    QML_FILES
        src/qml/MapView.qml
        # ... more QML files
)

# Windows specific
if(WIN32)
    set_target_properties(WardPlanning PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# macOS specific
if(APPLE)
    set_target_properties(WardPlanning PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist
    )
endif()
```

---

## 14. Testing Strategy

### Test Categories

1. **Unit Tests (models/)** - Test data model operations
2. **Command Tests (commands/)** - Test execute/undo
3. **Service Tests (services/)** - Test business logic
4. **Integration Tests** - Test component interactions
5. **UI Tests** - Test widget behavior

### Example Test

```cpp
// DocumentTest.cpp
#include <gtest/gtest.h>
#include "models/Document.h"
#include "models/Family.h"

class DocumentTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_document = Document::empty();
    }

    Document m_document;
};

TEST_F(DocumentTest, AddFamily) {
    Family family("id1", "Smith Family");

    auto newDoc = m_document.withFamilies({family});

    EXPECT_EQ(newDoc.families().size(), 1);
    EXPECT_EQ(newDoc.families()[0].name(), "Smith Family");

    // Original unchanged (immutable)
    EXPECT_EQ(m_document.families().size(), 0);
}

TEST_F(DocumentTest, JsonRoundTrip) {
    Family family("id1", "Smith Family");
    auto doc = Document::empty().withFamilies({family});

    auto json = doc.toJson();
    auto restored = Document::fromJson(json);

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->families().size(), 1);
    EXPECT_EQ(restored->families()[0].id(), "id1");
}
```

---

## 15. Migration Phases

### Phase 1: Foundation (Weeks 1-2)
**Goal: Project setup and core data models**

- [ ] Create CMake project structure
- [ ] Configure Qt dependencies
- [ ] Implement all data model classes
  - [ ] Document
  - [ ] Family
  - [ ] Person
  - [ ] Team
  - [ ] Tag
  - [ ] MinisteringVisit
  - [ ] EQMinisteringGroup / RSMinisteringGroup
  - [ ] MinisteringDistrict
  - [ ] ResourceType / ResourceCategory
  - [ ] Enums (Gender, ViewMode, MarkerDecorationType)
- [ ] Implement JSON serialization for all models
- [ ] Write unit tests for models
- [ ] Implement JsonService for file I/O

### Phase 2: Command Pattern & State (Weeks 3-4)
**Goal: Full undo/redo support**

- [ ] Implement Command base class
- [ ] Implement CommandHistory
- [ ] Implement all command classes
  - [ ] FamilyCommands
  - [ ] TeamCommands
  - [ ] MinisteringCommands
  - [ ] TagCommands
  - [ ] ResourceCommands
  - [ ] ImportCommands
- [ ] Implement DocumentManager
- [ ] Write command tests

### Phase 3: Services (Weeks 5-6)
**Goal: Business logic layer**

- [ ] FilterService
- [ ] StatisticsService
- [ ] ResourceService
- [ ] SearchService
- [ ] GeocodingService
- [ ] PreferencesService
- [ ] TileCacheService
- [ ] PdfImportService (basic)
- [ ] PdfReportService (basic)
- [ ] Write service tests

### Phase 4: Core UI (Weeks 7-9)
**Goal: Main window and navigation**

- [ ] MainWindow with menu bar
- [ ] Sidebar with tab navigation
- [ ] WardListView (family list)
- [ ] FamilyListModel (QAbstractListModel)
- [ ] SearchField widget
- [ ] FilterChips widget
- [ ] FamilyTile widget
- [ ] Basic FamilyDialog

### Phase 5: Map Integration (Weeks 10-11)
**Goal: Interactive map display**

- [ ] QML MapView component
- [ ] MapWidget (QQuickWidget wrapper)
- [ ] Marker rendering
- [ ] Tile caching integration
- [ ] Family highlighting
- [ ] Click handling
- [ ] Center-on-family

### Phase 6: Dialogs & Forms (Weeks 12-13)
**Goal: Complete editing UI**

- [ ] Complete FamilyDialog
- [ ] PersonForm
- [ ] VisitDialog
- [ ] StatisticsDialog
- [ ] TagDialog
- [ ] ResourceDialog
- [ ] ImportDialog

### Phase 7: Advanced Features (Weeks 14-16)
**Goal: Ministering and emergency views**

- [ ] MinisteringSidebar (EQ/RS views)
- [ ] DistrictTreeModel
- [ ] Visit tracking UI
- [ ] EmergencyView
- [ ] TeamsView
- [ ] ResourceView
- [ ] TagsView

### Phase 8: PDF & Import (Weeks 17-18)
**Goal: Full PDF capabilities**

- [ ] Complete PdfImportService
- [ ] Ward directory parsing
- [ ] Ministering PDF parsing
- [ ] Auto-detection (EQ vs RS)
- [ ] Complete PdfReportService
- [ ] All report types

### Phase 9: Polish & Testing (Weeks 19-20)
**Goal: Production ready**

- [ ] RF LOS calculations
- [ ] Background geocoding
- [ ] Offline indicator
- [ ] Window state persistence
- [ ] Keyboard shortcuts
- [ ] Comprehensive testing
- [ ] Performance optimization
- [ ] Documentation

### Phase 10: Packaging (Week 21)
**Goal: Distributable application**

- [ ] Windows installer (NSIS or WiX)
- [ ] macOS bundle
- [ ] Linux AppImage
- [ ] Code signing (if applicable)

---

## 16. Risk Assessment

### High Risk Items

| Risk | Mitigation |
|------|------------|
| Map library compatibility | Early prototype; fallback to simpler solution |
| PDF parsing accuracy | Extensive testing with real ward PDFs |
| Qt version availability | Target Qt 6.5 LTS |
| Performance with large wards | Profile early; optimize data structures |

### Medium Risk Items

| Risk | Mitigation |
|------|------------|
| QML learning curve | Start with simple components; reference Qt examples |
| Offline tile complexity | Simplify if needed; minimal viable caching first |
| Cross-platform differences | Test on all platforms throughout development |

### Low Risk Items

| Risk | Mitigation |
|------|------------|
| JSON serialization | Well-supported by Qt |
| Signal/slot migration | Direct mapping from Riverpod |
| Build system | CMake is mature and well-documented |

---

## 17. Appendix: File-by-File Mapping

### Models

| Flutter File | Qt File |
|--------------|---------|
| `lib/models/document.dart` | `src/models/Document.h/cpp` |
| `lib/models/family.dart` | `src/models/Family.h/cpp` |
| `lib/models/person.dart` | `src/models/Person.h/cpp` |
| `lib/models/team.dart` | `src/models/Team.h/cpp` |
| `lib/models/tag.dart` | `src/models/Tag.h/cpp` |
| `lib/models/ministering_visit.dart` | `src/models/MinisteringVisit.h/cpp` |
| `lib/models/eq_ministering_group.dart` | `src/models/EQMinisteringGroup.h/cpp` |
| `lib/models/rs_ministering_group.dart` | `src/models/RSMinisteringGroup.h/cpp` |
| `lib/models/ministering_district.dart` | `src/models/MinisteringDistrict.h/cpp` |
| `lib/models/ministering_group_base.dart` | `src/models/MinisteringGroupBase.h/cpp` |
| `lib/models/resource_type.dart` | `src/models/ResourceType.h/cpp` |
| `lib/models/resource_category.dart` | `src/models/ResourceCategory.h/cpp` |
| `lib/models/view_mode.dart` | `src/models/ViewMode.h` |
| `lib/models/marker_decoration_type.dart` | `src/models/MarkerDecorationType.h` |

### Commands

| Flutter File | Qt File |
|--------------|---------|
| `lib/commands/command.dart` | `src/commands/Command.h` |
| `lib/commands/family_commands.dart` | `src/commands/FamilyCommands.h/cpp` |
| `lib/commands/team_commands.dart` | `src/commands/TeamCommands.h/cpp` |
| `lib/commands/ministering_commands.dart` | `src/commands/MinisteringCommands.h/cpp` |
| `lib/commands/tag_commands.dart` | `src/commands/TagCommands.h/cpp` |
| `lib/commands/resource_type_commands.dart` | `src/commands/ResourceCommands.h/cpp` |
| `lib/commands/resource_assignment_commands.dart` | `src/commands/ResourceCommands.h/cpp` |
| `lib/commands/category_commands.dart` | `src/commands/ResourceCommands.h/cpp` |
| `lib/commands/import_commands.dart` | `src/commands/ImportCommands.h/cpp` |

### Services

| Flutter File | Qt File |
|--------------|---------|
| `lib/services/document_manager.dart` | `src/services/DocumentManager.h/cpp` |
| `lib/services/json_service.dart` | `src/services/JsonService.h/cpp` |
| `lib/services/pdf_import_service.dart` | `src/services/PdfImportService.h/cpp` |
| `lib/services/ministering_pdf_import_service.dart` | `src/services/PdfImportService.h/cpp` |
| `lib/services/pdf_report_service.dart` | `src/services/PdfReportService.h/cpp` |
| `lib/services/filter_service.dart` | `src/services/FilterService.h/cpp` |
| `lib/services/resource_service.dart` | `src/services/ResourceService.h/cpp` |
| `lib/services/statistics_service.dart` | `src/services/StatisticsService.h/cpp` |
| `lib/services/background_geocoding_service.dart` | `src/services/GeocodingWorker.h/cpp` |
| `lib/services/offline_tile_service.dart` | `src/services/TileCacheService.h/cpp` |
| `lib/services/preferences_service.dart` | `src/services/PreferencesService.h/cpp` |
| `lib/services/geocoding_service.dart` | `src/services/GeocodingService.h/cpp` |
| `lib/services/rf_los_service.dart` | `src/services/RfLosService.h/cpp` |
| `lib/services/elevation_cache_service.dart` | `src/services/ElevationCacheService.h/cpp` |
| `lib/services/search_service.dart` | `src/services/SearchService.h/cpp` |
| `lib/services/change_detection_service.dart` | (Built into CommandHistory) |

### Widgets

| Flutter File | Qt File |
|--------------|---------|
| `lib/screens/main_screen.dart` | `src/widgets/MainWindow.h/cpp` |
| `lib/widgets/sidebar.dart` | `src/widgets/Sidebar.h/cpp` |
| `lib/widgets/map_view.dart` | `src/qml/MapView.qml` + `src/widgets/MapWidget.h/cpp` |
| `lib/widgets/ward_list_view.dart` | `src/widgets/WardListView.h/cpp` |
| `lib/widgets/selection_view.dart` | `src/widgets/SelectionView.h/cpp` |
| `lib/widgets/family_dialog.dart` | `src/widgets/dialogs/FamilyDialog.h/cpp` |
| `lib/widgets/family_dialog/*` | `src/widgets/dialogs/PersonForm.h/cpp` |
| `lib/widgets/ministering_sidebar_content.dart` | `src/widgets/MinisteringSidebar.h/cpp` |
| `lib/widgets/ministering_visit_dialog.dart` | `src/widgets/dialogs/VisitDialog.h/cpp` |
| `lib/widgets/pdf_import_dialog.dart` | `src/widgets/dialogs/ImportDialog.h/cpp` |
| `lib/widgets/emergency_response_view.dart` | `src/widgets/EmergencyView.h/cpp` |
| `lib/widgets/skills_resources_view.dart` | `src/widgets/ResourceView.h/cpp` |
| `lib/widgets/tags_view.dart` | `src/widgets/TagsView.h/cpp` |
| `lib/widgets/teams_view.dart` | `src/widgets/TeamsView.h/cpp` |
| `lib/widgets/statistics_dialog.dart` | `src/widgets/dialogs/StatisticsDialog.h/cpp` |
| `lib/widgets/tag_*.dart` | `src/widgets/dialogs/TagDialog.h/cpp` |
| `lib/widgets/add_resource_type_dialog.dart` | `src/widgets/dialogs/ResourceDialog.h/cpp` |
| `lib/widgets/shared/search_field_widget.dart` | `src/widgets/shared/SearchField.h/cpp` |
| `lib/widgets/shared/filter_section.dart` | `src/widgets/shared/FilterChips.h/cpp` |
| `lib/widgets/shared/family_list_tile.dart` | `src/widgets/shared/FamilyTile.h/cpp` |
| `lib/widgets/shared/person_list_tile.dart` | `src/widgets/shared/PersonTile.h/cpp` |
| `lib/widgets/shared/family_badges.dart` | `src/widgets/shared/BadgeWidget.h/cpp` |

---

## Summary

This conversion plan provides a comprehensive roadmap for migrating the Ward Planning application from Flutter to C++ Qt. Key considerations:

1. **Preserve Architecture**: The command pattern, immutable models, and service layer translate well to Qt
2. **Leverage Qt Strengths**: Use Qt's Model/View framework, signal/slot mechanism, and QML for the map
3. **Phased Approach**: Build foundation first, then layer features incrementally
4. **Testing**: Write tests alongside implementation to ensure correctness
5. **Performance**: Profile early, especially for map rendering and large datasets

The estimated timeline is 20-21 weeks for a complete, polished application with all features from the Flutter version.
