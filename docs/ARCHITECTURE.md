# Ward Planning - Architecture Guide

## Overview

This document describes the architecture of the Ward Planning application, a C++17/Qt 6 desktop application for managing ward/stake family data, emergency response teams, and resources.

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

### Saving

Every change `WardSession` makes - a command, an undo or redo, a replay, a new ward, an automatic batch or lookup that changed the document - is saved at once: the document and this device's state together, in one write of the ward's local file, sealed to the ward's access list. The session writes through a `WardStore`; the app's is `FileWardStore`, over the `wards` folder in app data (`AppData`, under `QStandardPaths::AppLocalDataLocation`), a sibling of the `keys` folder that holds this device's key file. A save that fails keeps the change in memory and emits `saveFailed`; the next save that succeeds writes it, `MainWindow` tries again when the app becomes active, and closing or switching to another ward asks first while a save is still failing.

---

## Model Layer

### Core Models

```cpp
Document
|-- families: QHash<FamilyId, Family>
|-- addressCoordinates: QHash<QString, Coordinates>  (keyed by Address::full())
|-- teams: QHash<TeamId, Team>
|-- emergencyAssets: QHash<EmergencyAssetId, EmergencyAsset>
|-- emergencyResponse: std::optional<EmergencyResponse>  (the emergency in progress, if any)
|-- districts (EQ/RS current/proposed): QHash<MinisteringDistrictId, MinisteringDistrict>
|-- groups (EQ/RS current/proposed): QHash<MinisteringGroupId, MinisteringGroup>
`-- metadata: DocumentMetadata (wards, stakes)
```

Collections use `QHash<Id, Model>` for O(1) lookup by ID and automatic uniqueness enforcement. Display order is computed at render time (sorted by name, etc.).

Coordinates belong to addresses: a family's place on the map is its address's entry in `addressCoordinates`, read through `Document::coordinatesFor(family.address())`. Families at one address share an entry, and a family whose address has none is unmapped.

A ward holds at most one emergency in progress, which holds its contact attempts and tasks, each carrying the id of the family it is for. They are the log of what happened, so each also records the names of what it involves as they stood when it was written: an attempt its family's and the reached person's, a task its family's, and an assignment its assignee's, person or team. The views, the command descriptions and the report show those names, and use the ids for the live views: notify reads the assignee's contact details, and the map's status is per family. A family's status is worked out from the attempts and tasks that carry its id: Needs Help while one of its tasks is unresolved, otherwise the outcome of its most recent attempt that has one, otherwise Not Contacted. The attempt, task and category commands write to the emergency in progress and conflict when there is none. Starting one while one is in progress changes nothing, so two responders who each start one offline end up in one emergency. `WardSession::endEmergency` writes the emergency's summary report, naming families, people and teams as the attempts, tasks and assignments recorded them, and a family with none of them as the document holds it, and then removes it.

`EmergencyManager` turns the document's changes into four signals: `emergencyStarted` (none in progress to one), `emergencyEnded` (one to none), `familyStatusChanged(familyId)` (one family's attempts or tasks), and `responseDataChanged` (the counts, the progress bar and the map). On a change to one family's attempts or tasks, the family tree, the task table and the teams tree each replace only that family's rows. On a start or an end, the family tree replaces each family's attempt and task rows and the task table and the teams tree rebuild; every view rebuilds on a full document change.

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
    static Family create(const Address& address, const QList<Person>& members);
    static Family createWithId(const FamilyId& id, const Address& address,
                               const QList<Person>& members);

    // Read-only access
    const FamilyId& id() const;
    const Address& address() const;
    const QList<Person>& members() const;
    QString displayName() const;

    // Mutation via setters
    void setAddress(const Address& address);
    void setMembers(const QList<Person>& members);

    // Serialization
    QJsonObject toJson() const;
    static Family fromJson(JsonReader json);

    // Value-based equality (all fields)
    bool operator==(const Family& other) const;
};
```

### File Format

[FORMAT.md](FORMAT.md) specifies every field of a ward file and of the device key file. Every reader is strict through `JsonReader`: a `fromJson` takes each field its `toJson` writes by its type, takes a field `toJson` leaves out when empty with an `optional...` call, and ends with `finish()`, which fails the read for any field left untaken. The first problem is kept with the path of the field it names, and a file with one reads nothing. Every `toJson` writes a collection held by key in key order and a set in sorted order, so one value always writes the same JSON.

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

    // The addresses this command gives a family, known at construction; empty by default
    virtual QList<Address> addressesGiven() const;

    // The ward units this command gives the document, known at construction; empty by default
    virtual QStringList unitsGiven() const;

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

`CommandRegistry` lists every command type once by its type name. It writes a command as its type, its description and its fields, and rebuilds it from that form with the type's `fromJson`. A journal entry naming a type it lacks refuses the file that holds it.

An update command holds a `FieldChange<T>` (before and after) for each field it changes, and nothing for the fields it leaves alone. A field holds for the command at either end: at before, the command sets it; at after, its result is already there. Any other value is a conflict.

### WardSession

```cpp
class WardSession : public QObject
{
    Q_OBJECT

public:
    // The identity opens this device's local files and the store holds them; the geocoder
    // and unit lookup answer the automatic updates; tests pass fakes
    WardSession(const Identity* identity, WardStore* store, Geocoder* geocoder, UnitLookup* unitLookup,
                QObject* parent);

    const Document& document() const;
    const Journal& journal() const;

    // The ward, its landed marks and this device's state, as a save writes them
    WardPayload payload() const;

    // A new ward holding content, its access list founded for userName on this device
    void createWard(const Document& content, const QString& userName, const QString& deviceName);

    // The ward this device holds under document, from its local file
    OpenResult openWard(const DocumentId& document);

    // How document's local file in store reads for identity: opened, or why not
    static LocalRead readLocal(const WardStore& store, const Identity& identity, const DocumentId& document);

    // Saves again when a save is pending; true when nothing is left unsaved
    bool retryPendingSave();

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

    // The ward's geocoding and ward and stake lookup
    AutomaticUpdates& automaticUpdates();

signals:
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();
    void saveFailed(const QString& problem);  // once, until a save succeeds again
};
```

`WardSession` holds one open ward: its document, its journal and its undo and redo stacks, and owns the ward's `AutomaticUpdates`, which works on the same document and whose document changes the session passes on as its own. It holds no global state, so tests create as many sessions as they need.

- **The commit step** asks the command's rule, then runs `execute`. It answers `Applied`, `Conflicted`, or `Refused` with the rule's wording, and `NothingToDo` for an empty command, which is what an edit that changed nothing builds. A command that applies is appended to the journal, empties the redo stack, and enters the undo stack when it is an `UndoableCommand`; the undo stack holds at most 100 commands, with the oldest dropped. A command that conflicts or is refused changes nothing.
- **The journal** (`Journal`) holds each applied command's serialised form under the next sequence number, in the order the commands were made.
- **Replay** rebuilds each journal entry with `CommandRegistry` and puts it through the rule and `execute` on a copy of the version, in order. When every entry applies, the copy becomes the document. The first entry that conflicts or is refused abandons the replay: the version becomes the document as it stands, the journal and the undo and redo stacks are emptied, and the result holds the abandoned changes as plain text, which the app writes to a file for the user to redo.
- **Undo** builds the inverse of the command on top of the undo stack and runs it through the commit step; when it applies, the command moves to the redo stack. **Redo** runs that command again through the commit step and moves it back. An undo whose inverse conflicts or is refused leaves the command where it was.
- **Automatic updates** (`AutomaticUpdates`: geocoding and ward and stake lookup) change the document outside the journal, since their results are derived data that is computed again whenever it is missing. They start when a ward opens (`run()`), and after a command (or an undo or redo) applies that gives a family an address the table of address coordinates lacks, or gives the document a ward unit that lacks its details or names a stake the document lacks; the session hands each applied command to `commandApplied()`, and a replay starts none. A batch requests each address some family has that the table lacks, once, skipping addresses with no street and those on the failed list, and maps the families at each address as its result arrives, adding the entry and never replacing one already there. When its last request answers, the batch adds again every held result the document lacks, so a document replaced mid-batch loses none, then drops every entry no family's address uses, and emits `applied` once if the batch changed the table. Ward and stake lookups run one at a time and apply as each completes. Addresses and unit numbers that fail, and wards a lookup finds without a chapel, go on the failed list, and are not tried again until their text changes; the list is kept in memory for the open ward and starts empty whenever a ward is created or opened. A lookup that changes the document emits `documentChanged` and `applied` once; a batch emits `documentChanged` for each family it maps and `applied` once at its end; one that changes nothing emits neither.
- **Typed coordinates** are a journalled command on the address's entry, `SetAddressCoordinatesCommand`, which sets the entry whatever it holds; its inverse, `RevertAddressCoordinatesCommand`, puts back what the entry held when the command was built, provided the entry still holds the typed value.

### Example Command

```cpp
// An edit to one family, built from the family as the edit opened it and as it was saved
class UpdateFamilyCommand : public UndoableCommand
{
public:
    // Empty when nothing differs; each cleanup holds the places in document that refer to
    // the members it names
    static UndoableCommandPtr fromEdit(const Family& opened, const Family& saved, const Document& document);

    CommandOutcome execute(Document& document) const override
    {
        std::optional<Family> family = document.findFamilyById(m_familyId);
        if (!family.has_value() || !m_change.holds(*family))
        {
            return CommandOutcome::Conflicted;
        }
        document.setFamily(m_change.appliedTo(*family));
        m_removedCleanup.apply(document);
        m_addedCleanup.restore(document);
        return CommandOutcome::Applied;
    }

    UndoableCommandPtr inverse() const override
    {
        return std::make_shared<UpdateFamilyCommand>(m_familyId, m_change.swapped(), m_removedCleanup, m_addedCleanup,
                                                     description().undone());
    }

private:
    FamilyId m_familyId;
    FamilyChange m_change;
    ReferenceCleanup m_addedCleanup;
    ReferenceCleanup m_removedCleanup;
};
```

`FamilyChange` holds the address change, a `MemberChange` per changed member, the members added with the index each has in the saved family, and the members removed as last seen with the index each was at. It changes only the family. Member order is not part of the start condition: a change inserts each addition at its index and moves no one else, and the inverse of a removal puts the member back where they were.

### References stay whole

`ReferenceCleanup` is how every removal keeps teams, assets, ministering groups and districts, and open tasks' assignments pointing at people, families and teams that exist. A resolved task keeps its assignee, as part of the emergency's log. It holds the people and families a removal takes out, or the team, and the places that referred to them when the command was built, each as that team, asset, group, district or task stood then. `apply` takes the removed ids out of every place that holds them when it runs, whatever it holds; `restore` puts them back into each held place that still stands as `apply` left it, and leaves the rest as they stand; it puts back only the removed people the document holds. A cleanup is in no start condition.

A command that both removes and adds people holds one cleanup for everything it removes and one for everything it adds, and runs them once, after all its changes to families have landed: it applies the removal cleanup, then restores the other. Its inverse exchanges the two, so the inverse of a removal restores the places the removal held.

- `UpdateFamilyCommand` holds the pair for the members its `FamilyChange` removes and adds. `FamilyCollectionChange`, which both imports hold, holds the pair for the families it removes and adds, their people, and the members each of its `FamilyChange`s removes and adds.
- `DeleteFamilyCommand` removes a family as last seen and applies its cleanup; its inverse, `RestoreFamilyCommand`, puts the family back and restores the cleanup, and never conflicts.
- `DeleteTeamCommand` unassigns every open task assigned to the team; `AddTeamCommand`, its inverse, assigns the held tasks again where they still stand.
- A command that can change places outside the one thing it names reports `DocumentChange::full()`.

---

## Service Layer

### DocumentManager

A thin wrapper over the app's one `WardSession`, which holds the ward in memory while the app runs and saves every change, and the list of the wards this device holds:

```cpp
class DocumentManager : public QObject
{
    Q_OBJECT

public:
    // This device's identity and the store of its ward files; it owns neither
    DocumentManager(const Identity* identity, FileWardStore* store, QObject* parent);

    const Document& document() const;  // the session's document

    // The session's commit step, undo and redo
    CommitResult executeCommand(CommandPtr command);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    QString undoHeading() const;
    QString redoHeading() const;

    // A ward of Document::empty() for userName, on this device named deviceName; and the
    // ward this device holds under document. Each starts the ward's automatic updates.
    void createWard(const QString& userName, const QString& deviceName);
    OpenResult openWard(const DocumentId& document);

    const DocumentId& documentId() const;
    bool retryPendingSave();

    QString filePath() const;             // the open ward's file
    WardPayload payload() const;          // the open ward as a save writes it
    QList<WardListEntry> wards() const;   // every ward the store holds, read on this device

signals:
    void documentChanged(const DocumentChange& change);       // forwarded from the session
    void undoStackChanged();                                  // forwarded from the session
    void saveFailed(const QString& problem);                  // forwarded from the session
    void commandNotApplied(const CommitResult& result);
    void geocodingProgressChanged(int completed, int total);  // forwarded from the session
    void geocodingFinished();                                 // forwarded from the session
};
```

`executeCommand` hands the command to `WardSession::commit` and returns its `CommitResult`. When it is `Conflicted` or `Refused`, it emits `commandNotApplied`, and `MainWindow` tells the user what needs doing again, from the result's description, or which rule refused it; `NothingToDo`, from an edit that changed nothing, does neither. `undo` and `redo` run the session's `undo` and `redo` the same way; an undo's result carries its inverse's description, *Undo: heading* over the original's lines. `main` first takes the per-user instance lock, `InstanceLock` over `instance.lock` in app data. When another Ward Planning holds it, the launch hands its update file, or nothing, to that one over a local socket named for the lock's path and exits; the holder starts listening once its window is shown, and brings the window forward and opens the update it is handed. `main` then builds the manager on this device's identity, which `KeyFile` reads from app data's `keys` folder, and on a `FileWardStore` over its `wards` folder. `createWard` creates a ward of `Document::empty()`, whose access list the session founds with the user and this device alone, and `openWard` opens one from its local file; the session starts its undo stack and failed list empty, and a new ward's journal too. `MainWindow` asks for this device's name on the first start and for the user's name when it creates a ward, keeps both and the last ward's id in `settings.json` in app data, and at startup opens the last ward, showing the ward list (`OpenWardDialog`) when there is none or it does not open. The ward list is `WardList::read` over the store's `documents()`, each ward read through `WardSession::readLocal`; `filePath()` is the open ward's file, which *Show in Folder* reveals through `ShowInFolder`. Started with `--developer`, the window adds a *Debugging* menu whose *Export Decrypted JSON...* writes `payload()`, the open ward as a save writes it. The window title is the document's `wardName()`, from the names of its wards, then the app's name: `<ward name> - Ward Planning`.

### Geocoder and UnitLookup

`WardSession` runs its automatic updates through two interfaces, which tests replace with fakes:

```cpp
class Geocoder : public QObject
{
    virtual void geocode(const QString& address) = 0;
signals:
    void resolved(const QString& address, const Coordinates& coordinates);
    void failed(const QString& address);
};

class UnitLookup : public QObject
{
    virtual void lookupWard(const QString& wardUnitNumber) = 0;
    virtual void lookupStake(const QString& stakeUnitNumber) = 0;
signals:
    void wardFound(const QString& wardUnitNumber, const Ward& ward);
    void stakeFound(const QString& stakeUnitNumber, const Stake& stake);
    void lookupFailed(const QString& unitNumber, const QString& error);
};
```

The app's `NominatimGeocoder` answers through `GeocodingService`, which calls OpenStreetMap's Nominatim with at most one request every 1.1 seconds; `UnitLookupService` reads the meetinghouse locator through headless Edge. `DocumentManager` creates both and hands them to its session.

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

### FamilyListView

Main sidebar list for browsing families:

```cpp
class FamilyListView : public QWidget
{
    // Owns its own Filter and FamilyListModel
    void setup(DocumentManager* documentManager);
    QString selectedFamilyId() const;
signals:
    void familySelected(const QString& id);
    void visibleFamiliesChanged(const QStringList& ids);
};
```

### PeoplePickerDialog

Modal dialog for selecting families or persons with split list + map view:

```cpp
class PeoplePickerDialog : public QDialog
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
- The format: `WardPlanningSpecReader` in `tests/spec`, built from `docs/FORMAT.md` with no Qt and none of the app's code, reads a ward file of every field the app writes, and `SpecReaderTest` runs it
- The instance lock: `WardPlanningLockTests` in `tests/lock` runs `LockTest`, which starts copies of the executable, each taking the lock through `InstanceLock` as its own process, and one refused the lock handing a message to the holder
