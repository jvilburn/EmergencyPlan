# Emergency Plan Qt - Architecture Guide

## Overview

This document describes the architecture of the Emergency Plan Qt application, a C++17/Qt 6 desktop application for managing ward/stake family data, emergency response teams, and resources.

---

## Core Principles

### 1. Mutable Data Models with Command Pattern
Data models use standard setters for mutation. All user-facing state changes go through Command objects, which enables undo/redo by storing before/after state.

### 2. Command Pattern
All state mutations go through Command objects:
- Each command modifies the Document in place
- Commands store state needed for undo
- CommandHistory manages the undo/redo stacks
- Provides audit trail via `description()`

### 3. Separation of Concerns
```
UI Layer (Widgets)
    │ Knows about DocumentManager and list models
    ▼
List Model Layer (QAbstractItemModel subclasses)
    │ Bridges Document data to Qt views
    ▼
Service Layer (Business logic)
    │ Import, geocoding, PDF parsing
    ▼
Command Layer (State mutations)
    │ Modify Document, support undo/redo
    ▼
Model Layer (Data structures)
    │ Mutable data with setters
    ▼
Persistence Layer (JSON/Files)
```

### 4. Qt Model/View
For list and tree displays:
- Data in QAbstractItemModel subclasses
- Views (QListView, QTreeView) are data-agnostic
- Roles expose data to views

---

## Data Flow

### Reading Data

```
┌─────────────┐    signals     ┌──────────────────┐    model/view  ┌──────────┐
│ Document    │◄──────────────│ DocumentManager  │◄──────────────│ Widget   │
│ (data)      │                │ (owns Document)  │               │          │
└─────────────┘                └──────────────────┘               └──────────┘
```

1. Widget reads data through DocumentManager or list models
2. DocumentManager provides `const Document&` access
3. List models rebuild when `documentChanged` fires

### Modifying Data

```
┌──────────┐  action   ┌──────────────────┐  execute   ┌─────────────────┐
│ Widget   │──────────►│ DocumentManager  │───────────►│ Command         │
└──────────┘           └──────────────────┘            └─────────────────┘
                              │                               │
                              │                      ┌────────▼────────┐
                              │                      │ Modify Document │
                              │                      │ in place        │
                              │                      └────────┬────────┘
                              │                               │
                       ┌──────▼──────┐                        │
                       │ Emit signals│◄───────────────────────┘
                       │ (changed)   │
                       └─────────────┘
```

1. User action triggers widget to create a Command
2. Widget calls `DocumentManager::executeCommand()`
3. CommandHistory stores command, calls `command->execute(document)`
4. Command modifies Document in place
5. DocumentManager emits `documentChanged(DocumentChange)`
6. List models and widgets update in response

---

## Model Layer

### Core Models

```cpp
Document
├── families: QHash<FamilyId, Family>
├── teams: QHash<TeamId, Team>
├── tags: QHash<TagId, Tag>
├── emergencyAssets: QHash<EmergencyAssetId, EmergencyAsset>
├── districts (EQ/RS current/proposed): QHash<MinisteringDistrictId, MinisteringDistrict>
├── groups (EQ/RS current/proposed): QHash<MinisteringGroupId, MinisteringGroup>
├── metadata: DocumentMetadata (wards, stakes)
└── emergencyResponse: EmergencyResponse (response mode state)
```

Collections use `QHash<Id, Model>` for O(1) lookup by ID and automatic uniqueness enforcement. Display order is computed at render time (sorted by name, etc.).

### Unified Models

Several models that were originally separate have been unified:

- **MinisteringGroup** — Single class with `createEQ()`/`createRS()` factories. Tracks both `familyIds` (EQ) and `ministeredPersonIds` (RS) with an `m_isRSFormat` flag.
- **EmergencyAsset** — Single class replacing separate Skill/Equipment. Uses a `ResponseArea` enum to categorize.

### Model Pattern

Models use standard setters and factory creation:

```cpp
class Family
{
public:
    // Factory methods
    static Family create();
    static Family createWithId(const FamilyId& id);

    // Read-only access
    const FamilyId& id() const;
    const QString& name() const;

    // Mutation via setters
    void setAddress(const Address& address);
    void setMembers(const QList<Person>& members);
    void setLocation(std::optional<double> lat, std::optional<double> lng);

    // Serialization
    QJsonObject toJson() const;
    static Family fromJson(const QJsonObject& json);

    // Value-based equality (all fields)
    bool operator==(const Family& other) const;
};
```

### Qt Model Adapters

For displaying in QListView/QTreeView. Models take a `DocumentManager*` and `Filter*`
and automatically rebuild when either changes:

```cpp
class FamilyListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        AddressRole,
        // ...
    };

    explicit FamilyListModel(DocumentManager* documentManager,
                             Filter* filter,
                             QObject* parent);

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private slots:
    void rebuild();

private:
    DocumentManager* m_documentManager;
    Filter* m_filter;
    QList<FamilyId> m_familyIds;  // Filtered and sorted
};
```

### Filter Class

Reusable filter for families and persons, located in `src/listmodels/`:

```cpp
class Filter : public QObject
{
    Q_OBJECT

public:
    void setSearchText(const QString& text);
    QString searchText() const;

    bool passes(const Document& doc, const Family& family) const;
    bool passes(const Document& doc, const Person& person) const;

signals:
    void changed();
};
```

---

## Command Layer

### Command Interface

```cpp
class Command
{
public:
    virtual ~Command() = default;

    // Modify document in place
    virtual void execute(Document& doc) = 0;
    virtual void undo(Document& doc) = 0;

    // For display in Edit menu
    virtual QString description() const = 0;

    // What kind of change this makes (for targeted rebuilds)
    virtual DocumentChange documentChange() const = 0;
};
```

### Command History

```cpp
class CommandHistory : public QObject
{
    Q_OBJECT

public:
    void execute(CommandPtr command, Document& document);
    DocumentChange undo(Document& document);
    DocumentChange redo(Document& document);

    bool canUndo() const;
    bool canRedo() const;
    bool isDirty() const;
    void markSaved();

signals:
    void canUndoChanged();
    void canRedoChanged();
    void dirtyChanged();
};
```

### Example Command

```cpp
class AddFamilyCommand : public Command
{
public:
    explicit AddFamilyCommand(Family family)
        : m_family(std::move(family)) {}

    void execute(Document& doc) override
    {
        doc.addFamily(m_family);
    }

    void undo(Document& doc) override
    {
        doc.removeFamily(m_family.id());
    }

    QString description() const override
    {
        return tr("Add Family: %1").arg(m_family.name());
    }
};
```

---

## Service Layer

### DocumentManager

Central coordinator that owns the Document and CommandHistory:

```cpp
class DocumentManager : public QObject
{
    Q_OBJECT

public:
    const Document& document() const;

    // Command execution
    void executeCommand(CommandPtr command);
    void undo();
    void redo();

    // File operations
    void newDocument();
    bool openDocument(const QString& path);
    bool saveDocument();
    bool saveDocumentAs(const QString& path);

signals:
    void documentChanged(const DocumentChange& change);
    void dirtyChanged();
    void filePathChanged();
};
```

### GeocodingService

Two-layer architecture for background geocoding:

```cpp
// Low-level: Nominatim API with rate limiting (1.1s between requests)
class GeocodingService : public QObject
{
    void geocodeAddress(const QString& address);
signals:
    void geocodingComplete(const GeocodingResult& result);
};

// High-level: queue-based batch processing
class BackgroundGeocodingService : public QObject
{
    void queueFamily(const Family& family);
    void stop();
signals:
    void progressUpdated(int completed, int total);
    void familyGeocoded(const QString& id, double lat, double lng);
    void finished();
};
```

DocumentManager triggers batch geocoding after PDF imports and queues individual families when addresses change.

---

## UI Layer

### Widget Structure

```cpp
class MainWindow : public QMainWindow
{
    // Menu bar, status bar, central splitter
    // Left: sidebar with tab views
    // Right: MapWidget
};
```

### WardListView

Main sidebar list for browsing families:

```cpp
class WardListView : public QWidget
{
    // Owns its own Filter and FamilyListModel
    void setup(DocumentManager* documentManager);
    QString selectedFamilyId() const;
signals:
    void familySelected(const QString& id);
    void visibleFamiliesChanged(const QStringList& ids);
};
```

### WardListDialog

Modal dialog for selecting families or persons with split list + map view:

```cpp
class WardListDialog : public QDialog
{
    enum Mode { FamilyMode, PersonMode };
    enum SelectionMode { SingleSelect, MultiSelect };

    // Static convenience methods
    static QString selectFamily(DocumentManager* dm, ...);
    static QStringList selectFamilies(DocumentManager* dm, ...);
    static QString selectPerson(DocumentManager* dm, ...);
    static QStringList selectPersons(DocumentManager* dm, ...);
};
```

### MapWidget (Pure C++)

Native QWidget map with QPainter rendering:

```cpp
class MapWidget : public QWidget
{
    void centerOnFamily(const QString& familyId);
    void fitAllFamilies();
    void setHighlightedFamilies(const QVariantMap& ids);
    void setVisibleFamilyIds(const QStringList& ids);
signals:
    void familyClicked(const QString& familyId);
};
```

### Tile Services

- **TileService** — Main orchestration: provider fallback, scaled tile generation, layer switching
- **TileFetchService** — Network fetching with multi-provider fallback and deduplication
- **TileDiskCacheService** — Directory-based disk cache with in-memory QSet index

---

## File Formats

### Document JSON Structure

```json
{
    "version": 3,
    "families": {
        "uuid-1": {
            "id": "uuid-1",
            "name": "Smith Family",
            "latitude": 40.123,
            "longitude": -111.456,
            "address": "123 Main St",
            "members": [...]
        }
    },
    "teams": {},
    "tags": {},
    "emergencyAssets": {},
    "eqDistrictsCurrent": {},
    "eqGroupsCurrent": {},
    "rsDistrictsCurrent": {},
    "rsGroupsCurrent": {}
}
```

---

## Error Handling

1. **Validation Errors**: Return error messages, don't throw
2. **File Errors**: Return std::optional or error enum
3. **Network Errors**: Emit error signals, fail silently for geocoding/tiles
4. **Programming Errors**: Use Q_ASSERT in debug builds

---

## Testing Guidelines

### Unit Tests
- Models: JSON round-trip, equality
- Commands: execute/undo symmetry
- Services: pure function behavior

### Integration Tests
- File I/O round-trip
- Command execution flow
- PDF import
