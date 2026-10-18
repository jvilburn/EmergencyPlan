# Emergency Plan Qt - Architecture Guide

## Overview

This document describes the architecture of the Emergency Plan Qt application, a C++17/Qt 6 desktop application for managing ward/stake family data, emergency response teams, and resources.

---

## Core Principles

### 1. Mutable Data Models with Command Pattern
Data models use standard setters for mutation. Every user-facing change to the document is a Command run through `WardSession`'s commit step, and undo runs the command's inverse through the same step.

### 2. Command Pattern
All state mutations go through Command objects:
- A command is immutable once constructed: it holds what the user saw (its start condition), what it sets, and its description
- `execute` applies where the start condition or the result holds and answers `Applied`; anywhere else it answers `Conflicted` and leaves the document untouched
- An `UndoableCommand` is one undo can take back, and its `inverse` builds the command that does; a command undo cannot take back derives from `Command` and has no inverse
- `refusal` gives the wording of a rule that refuses the command on a document
- `WardSession` runs every command through one commit step, keeps the journal of serialised commands, replays the journal onto another version of the document, and holds the undo and redo stacks
- `description()` is a heading and a line per value set, in the user's words

### 3. Separation of Concerns
```
UI Layer (Widgets)
    | Knows about DocumentManager and list models
    v
List Model Layer (QAbstractItemModel subclasses)
    | Bridges Document data to Qt views
    v
Service Layer (Business logic)
    | Import, geocoding, PDF parsing; WardSession's commit step, journal, replay and undo
    v
Command Layer (State mutations)
    | Change the Document where what the user saw still holds
    v
Model Layer (Data structures)
    | Mutable data with setters, held in memory by WardSession
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
+-------------+    signals    +------------------+   model/view   +----------+
| Document    |<--------------| DocumentManager  |<---------------| Widget   |
| (data)      |               | (owns Document)  |                |          |
+-------------+               +------------------+                +----------+
```

1. Widget reads data through DocumentManager or list models
2. DocumentManager provides `const Document&` access
3. List models rebuild when `documentChanged` fires

### Modifying Data

```
+----------+  command  +------------------+  commit  +-------------+
| Widget   |---------->| DocumentManager  |--------->| WardSession |
+----------+           +------------------+          +------+------+
                                                            |
                                                rule, execute, journal
                                                            |
                                                   +--------v---------+
                                                   | Document changed |
                                                   | in place         |
                                                   +--------+---------+
                                                            |
                       +--------------+                     |
                       | Emit signals |<--------------------+
                       | (changed)    |
                       +--------------+
```

1. A user action has the widget build a Command from what it opened and what the user saved
2. The widget calls `DocumentManager::executeCommand()`, which hands the command to `WardSession::commit`
3. The commit step asks the command's rule (`refusal`), then runs `execute`
4. A command whose start condition or result holds changes the Document in place and answers `Applied`; the session appends it to the journal and, when it is an `UndoableCommand`, to the undo stack
5. `WardSession` emits `documentChanged(DocumentChange)` and `undoStackChanged()`
6. List models and widgets update in response

A command that conflicts or is refused changes nothing, and the result says which, so the app can tell the user.

---

## Model Layer

### Core Models

```cpp
Document
|-- families: QHash<FamilyId, Family>
|-- teams: QHash<TeamId, Team>
|-- tags: QHash<TagId, Tag>
|-- emergencyAssets: QHash<EmergencyAssetId, EmergencyAsset>
|-- districts (EQ/RS current/proposed): QHash<MinisteringDistrictId, MinisteringDistrict>
|-- groups (EQ/RS current/proposed): QHash<MinisteringGroupId, MinisteringGroup>
`-- metadata: DocumentMetadata (wards, stakes)
```

Collections use `QHash<Id, Model>` for O(1) lookup by ID and automatic uniqueness enforcement. Display order is computed at render time (sorted by name, etc.).

### Unified Models

Several models that were originally separate have been unified:

- **MinisteringGroup** - Single class with `createEQ()`/`createRS()` factories. Tracks both `familyIds` (EQ) and `ministeredPersonIds` (RS) with an `m_isRSFormat` flag.
- **EmergencyAsset** - Single class replacing separate Skill/Equipment. Uses a `ResponseArea` enum to categorize.

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
enum class CommandOutcome
{
    Applied,
    Conflicted
};

using CommandPtr = std::shared_ptr<const Command>;
using UndoableCommandPtr = std::shared_ptr<const UndoableCommand>;

class Command
{
public:
    virtual ~Command() = default;

    // The name this type has in the journal and in CommandRegistry
    virtual QString type() const = 0;

    // Applied where the start condition or the result holds; otherwise Conflicted,
    // with the document untouched
    virtual CommandOutcome execute(Document& document) const = 0;

    // The wording of the rule that refuses this command on this document, or empty
    virtual QString refusal(const Document& document) const;

    // This command's own fields, for the journal
    virtual QJsonObject toJson() const = 0;

    // What a run changes (for targeted rebuilds)
    virtual DocumentChange documentChange() const = 0;

    // A heading and a line per value set, fixed when the command is built
    const CommandDescription& description() const;
};

// A command undo can take back; the undo and redo stacks hold only these
class UndoableCommand : public Command
{
public:
    // The command that takes this one back, headed "Undo: <heading>"; never empty
    virtual UndoableCommandPtr inverse() const = 0;
};
```

`CommandRegistry` lists every command type once by its type name. It writes a command as its type, its description and its fields, and rebuilds it from that form with the type's `fromJson`.

An update command holds a `FieldChange<T>` (before and after) for each field it changes, and nothing for the fields it leaves alone. A field holds for the command at either end: at before, the command sets it; at after, its result is already there. Any other value is a conflict.

### WardSession

```cpp
class WardSession : public QObject
{
    Q_OBJECT

public:
    const Document& document() const;
    const Journal& journal() const;

    void open(const Document& document);

    // The commit step: the rule, execute, the journal
    CommitResult commit(CommandPtr command);

    // The top undo entry's inverse through the commit step; the top redo entry again
    CommitResult undo();
    CommitResult redo();
    bool canUndo() const;
    bool canRedo() const;
    QString undoHeading() const;
    QString redoHeading() const;

    // The journal replayed onto another version of the document
    ReplayResult replayOnto(const Document& version);

signals:
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();
};
```

`WardSession` holds one open ward: its document, its journal and its undo and redo stacks. It holds no global state, so tests create as many sessions as they need.

- **The commit step** asks the command's rule, then runs `execute`. It answers `Applied`, `Conflicted`, or `Refused` with the rule's wording, and `NothingToDo` for an empty command, which is what an edit that changed nothing builds. A command that applies is appended to the journal, empties the redo stack, and enters the undo stack when it is an `UndoableCommand`; the undo stack holds at most 100 commands, with the oldest dropped. A command that conflicts or is refused changes nothing.
- **The journal** (`Journal`) holds each applied command's serialised form under the next sequence number, in the order the commands were made.
- **Replay** rebuilds each journal entry with `CommandRegistry` and puts it through the rule and `execute` on a copy of the version, in order. When every entry applies, the copy becomes the document. The first entry that conflicts or is refused abandons the replay: the version becomes the document as it stands, the journal and the undo and redo stacks are emptied, and the result holds the abandoned changes as plain text, which the app writes to a file for the user to redo.
- **Undo** builds the inverse of the command on top of the undo stack and runs it through the commit step; when it applies, the command moves to the redo stack. **Redo** runs that command again through the commit step and moves it back. An undo whose inverse conflicts or is refused leaves the command where it was.

### Example Command

```cpp
// An edit to one family, built from the family as the edit opened it and as it was saved
class UpdateFamilyCommand : public UndoableCommand
{
public:
    // Empty when nothing differs
    static UndoableCommandPtr fromEdit(const Family& opened, const Family& saved);

    CommandOutcome execute(Document& document) const override
    {
        std::optional<Family> family = document.findFamilyById(m_familyId);
        if (!family.has_value() || !m_change.holds(*family))
        {
            return CommandOutcome::Conflicted;
        }
        document.updateFamily(m_change.appliedTo(*family));
        return CommandOutcome::Applied;
    }

    UndoableCommandPtr inverse() const override
    {
        return std::make_shared<UpdateFamilyCommand>(m_familyId, m_change.swapped(), description().undone());
    }

private:
    FamilyId m_familyId;
    FamilyChange m_change;
};
```

`FamilyChange` holds the address change, a `MemberChange` per changed member, the members added with the index each has in the saved family, and the members removed as last seen with the index each was at. Member order is not part of the start condition: a change inserts each addition at its index and moves no one else, and the inverse of a removal puts the member back where they were.

---

## Service Layer

### DocumentManager

A thin wrapper over the app's one `WardSession`, which holds the ward in memory while the app runs:

```cpp
class DocumentManager : public QObject
{
    Q_OBJECT

public:
    const Document& document() const;  // the session's document

    // The session's commit step, undo and redo
    CommitResult executeCommand(CommandPtr command);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    QString undoHeading() const;
    QString redoHeading() const;

    // Opens Document::empty() in the session
    void newDocument();

signals:
    void documentChanged(const DocumentChange& change);  // forwarded from the session
    void undoStackChanged();                             // forwarded from the session
    void commandNotApplied(const CommitResult& result);
};
```

`executeCommand` hands the command to `WardSession::commit` and returns its `CommitResult`. When it is `Applied`, the manager looks up any ward missing its details; when it is `Conflicted` or `Refused`, it emits `commandNotApplied`, and `MainWindow` tells the user what needs doing again, from the result's description, or which rule refused it; `NothingToDo`, from an edit that changed nothing, does neither. `undo` and `redo` run the session's `undo` and `redo` the same way; an undo's result carries its inverse's description, *Undo: heading* over the original's lines. The app starts each launch with `Document::empty()` open in the session, and `newDocument` opens a fresh one; either way the session starts its journal and undo stack empty. The window title is the document's `wardName()`, from the names of its wards.

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

    // Static convenience methods
    static std::optional<FamilySelectionResult> selectFamilies(...);
    static std::optional<PersonSelectionResult> selectPersons(...);
    static std::optional<SpecialNeedEdit> editSpecialNeed(const std::optional<PersonId>& lockedPerson,
                                                          QWidget* parent);
};
```

`editSpecialNeed` is the "Special Need" dialog. Opened on an existing need, its person is locked; opened from empty space, picking a person reads that person's family and fills in their current note, which is when the edit takes its start condition. It returns the family as read then and the family with the note set, from which `NeedsSubView` builds `UpdateFamilyCommand::fromEdit`.

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

- **TileService** - Main orchestration: provider fallback, scaled tile generation, layer switching
- **TileFetchService** - Network fetching with multi-provider fallback and deduplication
- **TileDiskCacheService** - Directory-based disk cache with in-memory QSet index

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
- Commands: the round trip through the journal, a conflict where a field moved, an inverse that restores the document
- Services: pure function behavior

### Integration Tests
- The commit step, replay and undo (`WardSession`)
- PDF import
