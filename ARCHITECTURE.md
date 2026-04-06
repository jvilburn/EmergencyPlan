# Emergency Plan Qt - Architecture Guide

## Overview

This document describes the architecture of the Emergency Plan Qt application, a C++17/Qt 6 desktop application for managing ward/stake family data, emergency response teams, and resources.

---

## Core Principles

### 1. Immutable Data Models
All data models are immutable. Modifications return new instances, enabling:
- Safe undo/redo operations
- Predictable state changes
- Thread-safe data access
- Easy change detection

### 2. Command Pattern
All state mutations go through Command objects:
- Encapsulates operation and its inverse
- Enables full undo/redo
- Provides audit trail
- Decouples UI from state management

### 3. Separation of Concerns
```
UI Layer (Widgets/QML)
    │ Knows only about ViewModels
    ▼
ViewModel Layer (QObject controllers)
    │ Orchestrates services, executes commands
    ▼
Service Layer (Stateless utilities)
    │ Business logic, no state
    ▼
Command Layer (State mutations)
    │ Transform immutable models
    ▼
Model Layer (Data structures)
    │ Pure data, no behavior
    ▼
Persistence Layer (JSON/Files)
```

### 4. Qt Model/View
For list and tree displays:
- Data in QAbstractItemModel subclasses
- Views (QListView, QTreeView) are data-agnostic
- Roles expose data to views and QML

---

## Data Flow

### Reading Data

```
┌─────────────┐    signals     ┌──────────────┐    property     ┌──────────┐
│ Document    │◄──────────────│ DocumentVM   │◄────────────────│ Widget   │
│ (immutable) │                │ (Q_PROPERTY) │                 │ or QML   │
└─────────────┘                └──────────────┘                 └──────────┘
```

1. Widget reads Q_PROPERTY from ViewModel
2. ViewModel provides data from current Document
3. Widget displays data

### Modifying Data

```
┌──────────┐  action   ┌──────────────┐  execute   ┌─────────────────┐
│ Widget   │──────────►│ DocumentVM   │───────────►│ Command         │
└──────────┘           └──────────────┘            └─────────────────┘
                              │                            │
                              │                   ┌────────▼────────┐
                              │                   │ Document.with() │
                              │                   │ (new instance)  │
                              │                   └────────┬────────┘
                              │                            │
                       ┌──────▼──────┐             ┌───────▼───────┐
                       │ Update m_doc│◄────────────│ Return new    │
                       │ Emit signals│             │ Document      │
                       └─────────────┘             └───────────────┘
```

1. User action triggers slot on ViewModel
2. ViewModel creates Command
3. Command executes: `newDoc = command.execute(currentDoc)`
4. ViewModel stores new Document, emits change signals
5. Widgets update via property bindings

---

## Model Layer

### Core Models

```cpp
Document
├── families: std::vector<Family>
├── teams: std::vector<Team>
├── tags: std::vector<Tag>
├── categories: std::vector<ResourceCategory>
├── resourceTypes: std::vector<ResourceType>
├── eqDistrictsCurrent/Proposed: std::vector<MinisteringDistrict>
├── eqGroupsCurrent/Proposed: std::vector<EQMinisteringGroup>
├── rsDistrictsCurrent/Proposed: std::vector<MinisteringDistrict>
└── rsGroupsCurrent/Proposed: std::vector<RSMinisteringGroup>
```

### Immutable Pattern

```cpp
class Family {
public:
    // Read-only access
    QString id() const;
    QString name() const;

    // Immutable updates - return new instances
    Family withName(const QString& name) const {
        Family copy = *this;
        copy.m_name = name;
        return copy;
    }

    Family withMembers(std::vector<Person> members) const {
        Family copy = *this;
        copy.m_members = std::move(members);
        return copy;
    }

private:
    QString m_id;
    QString m_name;
    std::vector<Person> m_members;
};
```

### Qt Model Adapters

For displaying in QListView/QTreeView or QML. Models take a `DocumentManager*` and `Filter*`
and automatically rebuild when either changes:

```cpp
class FamilyListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        AddressRole,
        // ... more roles
    };

    // Constructor connects to document/filter changes
    explicit FamilyListModel(DocumentManager* documentManager,
                                Filter* filter,
                                QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent) const override {
        return static_cast<int>(m_familyIds.size());
    }

    QVariant data(const QModelIndex& index, int role) const override;

    // Access filtered family IDs
    QStringList familyIds() const;

private slots:
    // Rebuilds filtered list when document or filter changes
    void rebuild();

private:
    DocumentManager* m_documentManager;
    Filter* m_filter;
    QList<QString> m_familyIds;  // Filtered and sorted
};
```

### Filter Class

Reusable filter for families and persons, located in `src/listmodels/`:

```cpp
class Filter : public QObject {
    Q_OBJECT

public:
    void setSearchText(const QString& text);
    QString searchText() const;

    // Check if family/person passes current filter criteria
    bool passes(const Document& doc, const Family& family) const;
    bool passes(const Document& doc, const Person& person) const;

signals:
    void changed();  // Emitted when any filter criteria changes

private:
    QString m_searchText;
};
```

---

## Command Layer

### Command Interface

```cpp
class Command {
public:
    virtual ~Command() = default;

    // Transform document, return new state
    virtual Document execute(const Document& doc) = 0;

    // Reverse transformation
    virtual Document undo(const Document& doc) = 0;

    // For display in Edit menu
    virtual QString description() const = 0;
};
```

### Command History

```cpp
class CommandHistory : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)
    Q_PROPERTY(bool isDirty READ isDirty NOTIFY isDirtyChanged)

public:
    Document execute(std::unique_ptr<Command> cmd, const Document& doc) {
        Document newDoc = cmd->execute(doc);

        // Clear redo stack on new action
        m_redoStack.clear();

        // Add to undo stack
        m_undoStack.push_back(std::move(cmd));

        emitChanges();
        return newDoc;
    }

    Document undo(const Document& doc) {
        if (m_undoStack.empty()) return doc;

        auto cmd = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        Document newDoc = cmd->undo(doc);
        m_redoStack.push_back(std::move(cmd));

        emitChanges();
        return newDoc;
    }

    Document redo(const Document& doc) {
        if (m_redoStack.empty()) return doc;

        auto cmd = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        Document newDoc = cmd->execute(doc);
        m_undoStack.push_back(std::move(cmd));

        emitChanges();
        return newDoc;
    }

    void markSaved() {
        m_savedPosition = m_undoStack.size();
        emit isDirtyChanged();
    }

    bool isDirty() const {
        return m_undoStack.size() != m_savedPosition;
    }

signals:
    void canUndoChanged();
    void canRedoChanged();
    void isDirtyChanged();

private:
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
    size_t m_savedPosition = 0;
};
```

### Example Commands

```cpp
// AddFamilyCommand
class AddFamilyCommand : public Command {
public:
    explicit AddFamilyCommand(Family family)
        : m_family(std::move(family)) {}

    Document execute(const Document& doc) override {
        auto families = doc.families();
        families.push_back(m_family);
        return doc.withFamilies(std::move(families));
    }

    Document undo(const Document& doc) override {
        auto families = doc.families();
        std::erase_if(families, [this](const Family& h) {
            return h.id() == m_family.id();
        });
        return doc.withFamilies(std::move(families));
    }

    QString description() const override {
        return tr("Add Family: %1").arg(m_family.name());
    }

private:
    Family m_family;
};

// UpdateFamilyCommand (stores before/after)
class UpdateFamilyCommand : public Command {
public:
    UpdateFamilyCommand(Family before, Family after)
        : m_before(std::move(before)), m_after(std::move(after)) {}

    Document execute(const Document& doc) override {
        return replaceFamily(doc, m_after);
    }

    Document undo(const Document& doc) override {
        return replaceFamily(doc, m_before);
    }

private:
    static Document replaceFamily(const Document& doc, const Family& h) {
        auto families = doc.families();
        for (auto& existing : families) {
            if (existing.id() == h.id()) {
                existing = h;
                break;
            }
        }
        return doc.withFamilies(std::move(families));
    }

    Family m_before;
    Family m_after;
};
```

---

## Service Layer

Services are stateless utility classes with static or member functions.

### FilterService

```cpp
class FilterService {
public:
    struct Filters {
        QString searchText;
        std::vector<QString> teamIds;
        std::vector<QString> skillIds;
        std::vector<QString> tagIds;
        bool showUnmapped = true;
        std::optional<Gender> genderFilter;
    };

    static std::vector<Family> filter(
        const std::vector<Family>& families,
        const Filters& filters,
        const Document& doc);

    static bool matchesSearch(const Family& h, const QString& text);
    static bool matchesTeam(const Family& h, const std::vector<QString>& teamIds, const Document& doc);
};
```

### GeocodingService

Two-layer architecture for background geocoding:

```cpp
// Low-level service: Nominatim API with rate limiting
class GeocodingService : public QObject {
    Q_OBJECT

public:
    void geocodeAddress(const QString& address);
    bool hasPendingRequests() const;

signals:
    void geocodingComplete(const GeocodingResult& result);

private:
    QNetworkAccessManager* m_networkManager;
    QTimer* m_rateLimitTimer;  // 1.1s between requests
};

// High-level service: queue-based batch processing
class BackgroundGeocodingService : public QObject {
    Q_OBJECT

public:
    void queueFamily(const Family& family);
    void stop();
    bool isRunning() const;

signals:
    void progressUpdated(int completed, int total);
    void familyGeocoded(const QString& id, double lat, double lng);
    void finished();
};
```

DocumentManager owns the BackgroundGeocodingService and:
- Triggers `startBatchGeocoding()` after PDF imports
- Triggers `queueFamilyForGeocoding()` when address changes
- Updates families silently (no undo/redo for geocoding)

---

## ViewModel Layer

ViewModels are QObject-based controllers that:
- Expose data via Q_PROPERTY
- Handle user actions via slots
- Emit signals when state changes

### DocumentViewModel

```cpp
class DocumentViewModel : public QObject {
    Q_OBJECT

    // Core data
    Q_PROPERTY(bool isDirty READ isDirty NOTIFY isDirtyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)
    Q_PROPERTY(QString currentFilePath READ currentFilePath NOTIFY filePathChanged)

public:
    explicit DocumentViewModel(QObject* parent = nullptr);

    // Document access
    const Document& document() const { return m_document; }

    // File operations
    Q_INVOKABLE void newDocument();
    Q_INVOKABLE bool open(const QString& path);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAs(const QString& path);

    // Undo/Redo
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Execute any command
    void executeCommand(std::unique_ptr<Command> command);

    // Family operations (convenience)
    Q_INVOKABLE void addFamily(const Family& h);
    Q_INVOKABLE void updateFamily(const Family& h);
    Q_INVOKABLE void deleteFamily(const QString& id);

signals:
    void documentChanged();
    void isDirtyChanged();
    void canUndoChanged();
    void canRedoChanged();
    void filePathChanged();

private:
    Document m_document;
    CommandHistory m_history;
    QString m_currentFilePath;

    void updateDocument(Document newDoc);
};
```

### FilterViewModel

```cpp
class FilterViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filtersChanged)
    Q_PROPERTY(QStringList selectedTeamIds READ selectedTeamIds NOTIFY filtersChanged)
    Q_PROPERTY(QStringList selectedTagIds READ selectedTagIds NOTIFY filtersChanged)
    Q_PROPERTY(bool showUnmapped READ showUnmapped WRITE setShowUnmapped NOTIFY filtersChanged)

public:
    FilterService::Filters toFilters() const;

    Q_INVOKABLE void toggleTeam(const QString& id);
    Q_INVOKABLE void toggleTag(const QString& id);
    Q_INVOKABLE void clearAll();

signals:
    void filtersChanged();

private:
    QString m_searchText;
    QSet<QString> m_selectedTeamIds;
    QSet<QString> m_selectedTagIds;
    bool m_showUnmapped = true;
};
```

---

## UI Layer

### Qt Widgets

For complex forms and native look:

```cpp
class FamilyDialog : public QDialog {
    Q_OBJECT

public:
    explicit FamilyDialog(QWidget* parent = nullptr);
    FamilyDialog(const Family& family, QWidget* parent = nullptr);

    Family result() const;

private slots:
    void onAddPerson();
    void onRemovePerson();
    void onGeocode();
    void onAccept();

private:
    void setupUi();
    void populate(const Family& h);
    void validate();

    QLineEdit* m_nameEdit;
    QLineEdit* m_addressEdit;
    QDoubleSpinBox* m_latSpinBox;
    QDoubleSpinBox* m_lngSpinBox;
    QListWidget* m_membersList;

    QString m_familyId;
    std::vector<Person> m_members;
};
```

### WardListView

The main sidebar list view for browsing families. Owns its own Filter and FamilyListModel:

```cpp
class WardListView : public QWidget {
    Q_OBJECT

public:
    explicit WardListView(QWidget* parent = nullptr);

    void setup(DocumentManager* documentManager);
    QString selectedFamilyId() const;
    void setSelectedFamilyId(const QString& id);
    QStringList visibleFamilyIds() const;

signals:
    void familySelected(const QString& id);
    void visibleFamiliesChanged(const QStringList& ids);

private:
    FilterableListWidget* m_listWidget;
    FamilyListModel* m_model;
    Filter* m_filter;  // Owned
};
```

### WardListDialog

Modal dialog for selecting families or persons. Features split view with list and map:

```cpp
class WardListDialog : public QDialog {
    Q_OBJECT

public:
    enum Mode { FamilyMode, PersonMode };
    enum SelectionMode { SingleSelect, MultiSelect };

    explicit WardListDialog(DocumentManager* documentManager,
                            Mode mode,
                            QWidget* parent = nullptr);

    void setSelectionMode(SelectionMode mode);
    void setPreselectedIds(const QStringList& ids);
    QStringList selectedIds() const;

    // Static convenience methods
    static QString selectFamily(DocumentManager* dm,
                                   const QString& initialId = {},
                                   QWidget* parent = nullptr);
    static QStringList selectFamilies(DocumentManager* dm,
                                        const QStringList& initialIds = {},
                                        QWidget* parent = nullptr);
    static QString selectPerson(DocumentManager* dm,
                                const QString& initialId = {},
                                QWidget* parent = nullptr);
    static QStringList selectPersons(DocumentManager* dm,
                                     const QStringList& initialIds = {},
                                     QWidget* parent = nullptr);

private:
    Mode m_mode;
    DocumentManager* m_documentManager;
    Filter* m_filter;
    FilterableListWidget* m_listWidget;
    MapWidget* m_mapWidget;
    FamilyListModel* m_familyModel;  // or PersonListModel based on mode
};
```

### MapWidget (Pure C++)

Native QWidget map implementation with QPainter rendering (replaces QML approach):

```cpp
class MapWidget : public QWidget {
    Q_OBJECT

public:
    explicit MapWidget(DocumentManager* docManager, QWidget* parent = nullptr);

    void centerOnFamily(const QString& familyId);
    void fitAllFamilies();
    void setHighlightedFamilies(const QVariantMap& ids);
    void setVisibleFamilyIds(const QStringList& ids);  // Filter de-emphasis

signals:
    void familyClicked(const QString& familyId);

private:
    // Tile rendering, marker drawing, animation, etc.
};
```

---

## Highlighting Architecture

To prevent cross-contamination between tabs:

### Pure Highlighting Providers

```cpp
// Each view has its own highlighting logic
class TeamHighlightProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList highlightedFamilyIds READ highlightedFamilyIds
               NOTIFY highlightedChanged)

public:
    void setSelectedTeam(const QString& teamId, const Document& doc);

signals:
    void highlightedChanged();

private:
    QStringList m_highlightedIds;
};

class MinisteringHighlightProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList highlightedFamilyIds READ highlightedFamilyIds
               NOTIFY highlightedChanged)

public:
    void setSelectedGroup(const QString& groupId, const Document& doc);
};
```

### Orchestrator

```cpp
class HighlightOrchestrator : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList activeHighlightedIds READ activeHighlightedIds
               NOTIFY activeHighlightedChanged)

public:
    void setViewMode(ViewMode mode);
    void setActiveTab(const QString& tabId);

    // Only the map watches this
    QStringList activeHighlightedIds() const;

signals:
    void activeHighlightedChanged();

private:
    ViewMode m_viewMode;
    QString m_activeTab;

    TeamHighlightProvider m_teamProvider;
    MinisteringHighlightProvider m_ministeringProvider;
    // ... other providers

    QStringList selectActiveProvider() const;
};
```

---

## Threading

### Main Thread Only
- All UI (widgets, QML)
- ViewModels
- Signal/slot connections (by default)

### Background Threads
- Geocoding batches
- Tile downloading
- PDF parsing (optional)
- RF LOS calculations

### Thread Communication

```cpp
// Worker in separate thread
class GeocodingWorker : public QObject {
    Q_OBJECT

public slots:
    void processAddresses(const QStringList& addresses);

signals:
    void progress(int current, int total);
    void addressGeocoded(QString address, double lat, double lng);
    void finished();
};

// Usage
auto thread = new QThread;
auto worker = new GeocodingWorker;
worker->moveToThread(thread);

connect(this, &MyClass::startGeocoding, worker, &GeocodingWorker::processAddresses);
connect(worker, &GeocodingWorker::addressGeocoded, this, &MyClass::onGeocoded);
connect(worker, &GeocodingWorker::finished, thread, &QThread::quit);
connect(thread, &QThread::finished, worker, &QObject::deleteLater);

thread->start();
```

---

## File Formats

### Document JSON Structure

```json
{
    "version": 3,
    "families": [
        {
            "id": "uuid-1",
            "name": "Smith Family",
            "latitude": 40.123,
            "longitude": -111.456,
            "address": "123 Main St",
            "members": [
                {
                    "id": "uuid-2",
                    "name": "Smith, John",
                    "phone": "555-1234",
                    "gender": "male",
                    "callings": ["EQ President"]
                }
            ],
            "ministeringVisits": []
        }
    ],
    "teams": [],
    "tags": [],
    "categories": [],
    "resourceTypes": [],
    "eqDistrictsCurrent": [],
    "eqGroupsCurrent": [],
    "rsDistrictsCurrent": [],
    "rsGroupsCurrent": []
}
```

### Tile Cache Structure

```
~/.emergencyplan/cache/tiles/
├── osm/
│   ├── 12/
│   │   ├── 1234/
│   │   │   ├── 5678.png
│   │   │   └── 5679.png
│   │   └── 1235/
│   └── 13/
└── satellite/
    └── ...
```

---

## Error Handling

### Strategy

1. **Validation Errors**: Return error messages, don't throw
2. **File Errors**: Return std::optional or error enum
3. **Network Errors**: Emit error signals
4. **Programming Errors**: Use Q_ASSERT in debug builds

### Example

```cpp
struct LoadResult {
    std::optional<Document> document;
    QString errorMessage;

    bool success() const { return document.has_value(); }
};

LoadResult JsonService::loadDocument(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return { std::nullopt, tr("Cannot open file: %1").arg(file.errorString()) };
    }

    QJsonParseError parseError;
    auto jsonDoc = QJsonDocument::fromJson(file.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return { std::nullopt, tr("Invalid JSON: %1").arg(parseError.errorString()) };
    }

    // ... parse and return document
    return { document, QString() };
}
```

---

## Performance Considerations

### Large Ward Optimization

1. **Virtual Lists**: Use QAbstractItemModel with lazy loading
2. **Incremental Updates**: Use `dataChanged()` instead of `resetModel()`
3. **Async Operations**: Background geocoding, tile loading
4. **Caching**: Cache computed values (statistics, filtered lists)

### Map Performance

1. **Marker Pooling**: Reuse marker items
2. **Level of Detail**: Simplify markers at low zoom
3. **Tile Caching**: Aggressive disk caching
4. **Culling**: Don't render off-screen markers

---

## Testing Guidelines

### Unit Tests
- Models: JSON round-trip, copyWith, equality
- Commands: execute/undo symmetry
- Services: pure function behavior

### Integration Tests
- ViewModel: command execution, signal emission
- Model adapters: role data correctness

### UI Tests
- Dialog: form validation
- Navigation: state persistence

### Example

```cpp
TEST(FamilyTest, JsonRoundTrip) {
    Family original("id1", "Smith Family");
    original = original.withAddress("123 Main St");

    auto json = original.toJson();
    auto restored = Family::fromJson(json);

    EXPECT_EQ(original.id(), restored.id());
    EXPECT_EQ(original.name(), restored.name());
    EXPECT_EQ(original.address(), restored.address());
}

TEST(CommandHistoryTest, UndoRedo) {
    CommandHistory history;
    Document doc = Document::empty();

    auto cmd = std::make_unique<AddFamilyCommand>(Family("id1", "Smith"));
    doc = history.execute(std::move(cmd), doc);

    EXPECT_EQ(doc.families().size(), 1);
    EXPECT_TRUE(history.canUndo());
    EXPECT_FALSE(history.canRedo());

    doc = history.undo(doc);
    EXPECT_EQ(doc.families().size(), 0);
    EXPECT_FALSE(history.canUndo());
    EXPECT_TRUE(history.canRedo());
}
```
