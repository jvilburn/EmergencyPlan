# Ward Planning - Architecture Guide

## Overview

This document describes the architecture of the Ward Planning application, a C++17/Qt 6 desktop application for managing ward/stake family data, emergency response teams, and resources. A ward is an encrypted document that can be shared through a server which holds who has access but never a key; the *Shared Wards* section describes that part. The full design is in `docs/plans/2026-08-30-encrypted-shared-document-design.md`.

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

    // Serialization: fromJson reads exactly what toJson writes, or nothing
    QJsonObject toJson() const;
    static std::optional<Family> fromJson(const QJsonObject& json);

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

signals:
    void canUndoChanged();
    void canRedoChanged();
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

The ward open in the window: it holds that ward's `DocumentSession`, which saves
and publishes, and keeps the CommandHistory. Every change reaches the document
through the session's edit, which refuses a directory change while another member
holds the lease:

```cpp
class DocumentManager : public QObject
{
    Q_OBJECT

public:
    const Document& document() const;

    // Command execution; false when refused
    bool executeCommand(CommandPtr command);
    void undo();
    void redo();

    // Wards, by document id, from the DocumentLibrary
    bool newDocument(QString* errorMessage);
    Open openDocument(const QString& documentId, QString* refusal);
    bool publishNow(QString* errorMessage);

signals:
    void documentChanged(const DocumentChange& change);
    void documentOpened();
    void editRefused(const QString& message);
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

- **TileService** - Main orchestration: provider fallback, scaled tile generation, layer switching
- **TileFetchService** - Network fetching with multi-provider fallback and deduplication
- **TileDiskCacheService** - Directory-based disk cache with in-memory QSet index

---

## Shared Wards

### The ward is its own access list

The server is a versioned store of encrypted documents with an edit lease. It holds no
key material, never reads a payload, and knows nothing about people. Who may read a ward
is whoever its current header seals the key to; who may write it is whoever can sign as
one of them. Every version it stores is ciphertext under a key sealed, in the version's
own header, to each device and open invite named in the payload's **roster**
(`SealingSet`): people with names and roles, their devices with their keys, and invites.
The header and the roster must agree exactly, and every reader checks that they do.

A write starts from the roster of the version it builds on. When the roster now lacks a
device or an invite that version sealed to, the data key rotates, so whoever was taken
out holds no key to anything written afterwards. Nothing else changes who can read.

### Invites

An invite is a one-time bearer key: a seed in a `wardplanning:join/<id>/<seed>` link, from
which the invite's sealing and signing keypairs are derived. Making one writes a version
sealed to the invite as well. Opening the link fetches the current version as the
invite, puts this device in the invite's place in the roster (under the inviter's person
for a Device invite, as a new person for a User invite), and writes the next version
signed by the invite, which the current header names. From then on the device signs as
itself. Roles are a convention among honest clients; the server cannot see them.

### The services

| Service | Role |
|---|---|
| `DocumentCrypto` | libsodium: sealing the data key to a device's X25519 key, AEAD over the compressed payload with the whole header as associated data, Ed25519 signatures on writes, keypairs from an invite's seed |
| `DocumentFile` | The on-disk and on-wire format: header line and ciphertext |
| `DocumentHeader` | The header's fields and strict parse; one format version covers the whole file |
| `SealingSet` | The roster inside the payload, and whether a write on top of a given header rotates the key |
| `RosterDiff` | What changed between two rosters, for the access notice |
| `Reconcile` | Validates a version before anything local changes: sealed to this device, decrypts, header matches the roster, descends from a key this device trusts |
| `ServerContract` | The server API as C++ types, five calls and signed writes; `FakeServer` in the tests and `HttpServerApi` implement it |
| `HttpServerApi`, `HttpWire` | HTTP transport and JSON shapes, one table in `HttpWire.h` |
| `ServerConnection` | Signs writes as this device, or as an invite while joining |
| `IdentityService`, `IdentityFile` | This device's keys and person, kept like SSH keys in local app data |
| `DocumentLibrary` | The wards on this device, named by document id in app data; copies; joining a ward |
| `DocumentSession` | One open ward: the single write path, save and publish, the base copy, the lease, adopting newer versions, conflicts, invites, leaving, lost access |
| `ServerVersion` | Taking in and writing server versions: the base copy, the sealing-set rule, why a version left this device out |
| `DocumentManager` | Commands, undo, and geocoding over the open session; roster edits; the access notice |
| `UpdateChecker` | Whether a newer release is published, from GitHub, apart from the Ward Planning server |

### The single write path

Every change to the open ward goes through `DocumentSession::edit`, which applies the
change to the in-memory `Document`, refuses a directory edit while another member holds
the lease, and schedules a save. Commands, undo, imports, and roster edits all enter
here. Leaving is the one write that does not touch the local file: the version without
this person is built in memory and uploaded, and the local copy goes once it has landed.

### Save and publish

Saving writes the local file a moment after a change, with the sealing-set rule applied
against the version it builds on; it never touches the network. Publishing uploads the
saved file conditional on the server version this session started from. The **base
copy** beside the local file is the file exactly as it stood at that version; its save
counter tells whether the local file has unpublished changes, and a three-way merge
against it tells what changed here from what changed there. The roster is directory
content for that merge: changed on one side, that side's roster wins; changed on both,
the conflict dialog. A refused publish fetches the server's version, validates it,
merges, reseals, and publishes again.

## File Formats

### The encrypted file

A `.wardplanning` file, on disk or on the server, is a header line of JSON followed by ciphertext. The header holds the format version, document id, save counter, nonce, and one recipient entry per device: its fingerprint and the data key sealed to it. The whole header is the payload's associated data, so any change to it fails decryption. The payload is the document JSON below, compressed and encrypted.

### Document JSON Structure

The payload `Document::toJson` writes and `Document::parse` reads. Collections are
arrays, each omitted when empty; the dates and the emergency response are omitted when
absent. `parse` accepts exactly this: an unknown field, a damaged element, or a repeated
id refuses the whole payload. Its format version lives in the file's header, not here.

```json
{
    "sealingSet": { "devices": [...], "formerKeys": [...] },
    "stakes": [...],
    "wards": [...],
    "families": [
        {
            "id": "uuid-1",
            "latitude": 40.123,
            "longitude": -111.456,
            "address": ["123 Main St", "Provo, UT 84601"],
            "members": [...]
        }
    ],
    "teams": [...],
    "tags": [...],
    "emergencyResources": [...],
    "eqDistricts": [...],
    "eqGroups": [...],
    "rsDistricts": [...],
    "rsGroups": [...],
    "wardDirectoryPdfDate": "2026-08-15",
    "ministeringPdfDate": "2026-08-20",
    "responseData": { ... }
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
