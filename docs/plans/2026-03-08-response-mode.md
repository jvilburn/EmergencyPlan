# Response Mode Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add emergency response tracking that augments existing views with contact status, tasks, and team coordination — without replacing the preparation workflow.

**Architecture:** Response data lives in the main document file under a `"responseData"` JSON key, logically isolated from preparation data. No undo/redo for response actions — mistakes are corrected by deleting entries or changing status back. Response data mutations go through `EmergencyManager` (not the command system), which calls `DocumentManager::setEmergencyResponse()` and triggers auto-save. Views check `EmergencyManager::isActive()` to show/hide response UI. Archives are full document snapshots (prep + response), self-contained and independent of the main file.

**Design doc deviation:** The design doc says "response data stored separately" (separate file). This plan stores it in the main document under a separate key instead. Rationale: reuses atomic `QSaveFile` save, avoids split-brain between two files, and simplifies auto-save. The design doc should be updated to reflect this.

**Tech Stack:** Qt 6 / C++17, existing Document/CommandHistory/MainWindow architecture

**Prerequisites (already complete):**
- Strong ID types (`FamilyId`, `PersonId`, `TeamId` in `src/models/Id.h`)
- Auto-save (fires on every document mutation)

---

## Phase 1: Data Model + Emergency Lifecycle

### Task 1.1: Response Data Models

Create the core data structures for emergency response tracking.

**Files:**
- Create: `src/models/EmergencyResponse.h`
- Create: `src/models/EmergencyResponse.cpp`

**New ID types** — add to `src/models/Id.h`:

```cpp
class TaskId : public IdBase<TaskId>
{
    friend class IdBase<TaskId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "TaskId"; }
};

class ContactAttemptId : public IdBase<ContactAttemptId>
{
    friend class IdBase<ContactAttemptId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "ContactAttemptId"; }
};
```

Add `Q_DECLARE_METATYPE` at the bottom of `Id.h`:

```cpp
Q_DECLARE_METATYPE(TaskId)
Q_DECLARE_METATYPE(ContactAttemptId)
```

**ContactStatus enum:**

```cpp
enum class ContactStatus
{
    NotContacted,
    OK,
    UnableToReach
    // "Needs Help" is derived from having unresolved tasks — not stored
};
```

Note: A family's effective status is `NeedsHelp` when `contactStatus != NotContacted` and it has unresolved tasks. This is computed, not stored.

**ContactAttempt class:**

```cpp
class ContactAttempt
{
public:
    static ContactAttempt create(ContactMethod method, const PersonId& who, const QString& notes);

    const ContactAttemptId& id() const { return m_id; }
    ContactMethod method() const { return m_method; }
    const PersonId& who() const { return m_who; }
    const QDateTime& timestamp() const { return m_timestamp; }
    const QString& notes() const { return m_notes; }

    void setMethod(ContactMethod method) { m_method = method; }
    void setNotes(const QString& notes) { m_notes = notes; }

    QJsonObject toJson() const;
    static ContactAttempt fromJson(const QJsonObject& json);
    bool operator==(const ContactAttempt& other) const;
    bool operator!=(const ContactAttempt& other) const;

private:
    ContactAttemptId m_id;
    ContactMethod m_method = ContactMethod::Phone;
    PersonId m_who;
    QDateTime m_timestamp;
    QString m_notes;
};
```

`create()` auto-sets `m_id` (generated) and `m_timestamp` (current UTC time). Notes are required if method is `Other`.

**ContactMethod enum:**

```cpp
enum class ContactMethod
{
    Phone,
    Text,
    Email,
    Visit,
    Other
};
```

**TaskNotification class:**

```cpp
class TaskNotification
{
public:
    static TaskNotification create(ContactMethod method, const QString& notes);

    ContactMethod method() const { return m_method; }
    const QDateTime& timestamp() const { return m_timestamp; }
    const QString& notes() const { return m_notes; }

    QJsonObject toJson() const;
    static TaskNotification fromJson(const QJsonObject& json);
    bool operator==(const TaskNotification& other) const;
    bool operator!=(const TaskNotification& other) const;

private:
    ContactMethod m_method = ContactMethod::Phone;
    QDateTime m_timestamp;
    QString m_notes;
};
```

`create()` auto-sets `m_timestamp` to current UTC time.

**ResponseTask class:**

```cpp
class ResponseTask
{
public:
    static ResponseTask create(const QString& category, const QString& description);

    const TaskId& id() const { return m_id; }
    const QString& category() const { return m_category; }
    const QString& description() const { return m_description; }
    const QDateTime& createdAt() const { return m_createdAt; }

    // Assignment — at most one set
    const std::optional<TeamId>& assignedTeamId() const { return m_assignedTeamId; }
    const std::optional<PersonId>& assignedPersonId() const { return m_assignedPersonId; }
    const QString& assignmentNotes() const { return m_assignmentNotes; }
    const std::optional<TaskNotification>& notification() const { return m_notification; }

    void assignToTeam(const TeamId& teamId, const QString& notes);
    void assignToPerson(const PersonId& personId, const QString& notes);
    void clearAssignment();
    void setNotification(const TaskNotification& notification);

    // Resolution
    bool isResolved() const { return m_resolved; }
    const QString& resolutionNotes() const { return m_resolutionNotes; }
    const std::optional<QDateTime>& resolvedAt() const { return m_resolvedAt; }

    void resolve(const QString& notes);
    void unresolve();

    bool isAssigned() const;
    bool isNotified() const;

    QJsonObject toJson() const;
    static ResponseTask fromJson(const QJsonObject& json);
    bool operator==(const ResponseTask& other) const;
    bool operator!=(const ResponseTask& other) const;

private:
    TaskId m_id;
    QString m_category;
    QString m_description;
    QDateTime m_createdAt;

    std::optional<TeamId> m_assignedTeamId;
    std::optional<PersonId> m_assignedPersonId;
    QString m_assignmentNotes;
    std::optional<TaskNotification> m_notification;

    bool m_resolved = false;
    QString m_resolutionNotes;
    std::optional<QDateTime> m_resolvedAt;
};
```

`create()` auto-sets `m_id` (generated) and `m_createdAt` (current UTC time). `assignToTeam`/`assignToPerson` clear the other assignment type (at most one). `resolve()` sets `m_resolved = true` and `m_resolvedAt` to current UTC time.

**FamilyResponseRecord class** (per-family response data):

```cpp
class FamilyResponseRecord
{
public:
    static FamilyResponseRecord create(const FamilyId& familyId,
                                       const QString& displayName,
                                       const QString& address);

    const FamilyId& familyId() const { return m_familyId; }
    const QString& displayName() const { return m_displayName; }
    const QString& address() const { return m_address; }
    ContactStatus contactStatus() const { return m_contactStatus; }
    const QList<ContactAttempt>& contactAttempts() const { return m_contactAttempts; }
    const QList<ResponseTask>& tasks() const { return m_tasks; }

    void setContactStatus(ContactStatus status) { m_contactStatus = status; }
    void addContactAttempt(const ContactAttempt& attempt);
    void removeContactAttempt(const ContactAttemptId& id);
    void addTask(const ResponseTask& task);
    void updateTask(const ResponseTask& task);
    void removeTask(const TaskId& id);

    // Derived status: NeedsHelp when contacted but has unresolved tasks
    bool needsHelp() const;
    int unresolvedTaskCount() const;

    // Effective status for display (combines stored + derived)
    // Returns NeedsHelp if contactStatus != NotContacted and has unresolved tasks,
    // otherwise returns contactStatus mapped to EffectiveContactStatus
    EffectiveContactStatus effectiveStatus() const;

    QJsonObject toJson() const;
    static FamilyResponseRecord fromJson(const QJsonObject& json);
    bool operator==(const FamilyResponseRecord& other) const;
    bool operator!=(const FamilyResponseRecord& other) const;

private:
    FamilyId m_familyId;
    QString m_displayName;       // snapshot at emergency start, not updated
    QString m_address;           // snapshot at emergency start, not updated
    ContactStatus m_contactStatus = ContactStatus::NotContacted;
    QList<ContactAttempt> m_contactAttempts;  // most recent first
    QList<ResponseTask> m_tasks;             // most recent first
};
```

`displayName` and `address` are **set once at emergency start** from the family's current data. They are not updated if the family is edited during the emergency. This ensures archives preserve the name/address as they were when the emergency began.

For `effectiveStatus()`, introduce a display enum:

```cpp
enum class EffectiveContactStatus
{
    NotContacted,
    OK,
    NeedsHelp,      // derived
    UnableToReach
};
```

**EmergencyResponse class** (container for all response data):

```cpp
class EmergencyResponse
{
public:
    static EmergencyResponse create(const QString& name);

    const QString& name() const { return m_name; }
    const QDateTime& startedAt() const { return m_startedAt; }
    const std::optional<QDateTime>& endedAt() const { return m_endedAt; }
    const QHash<FamilyId, FamilyResponseRecord>& familyRecords() const { return m_familyRecords; }
    const QStringList& taskCategories() const { return m_taskCategories; }

    void setEndedAt(const QDateTime& endedAt) { m_endedAt = endedAt; }

    // Family record management (called by EmergencyManager)
    void addFamilyRecord(const FamilyResponseRecord& record);
    FamilyResponseRecord* mutableRecord(const FamilyId& familyId);
    const FamilyResponseRecord* findRecord(const FamilyId& familyId) const;

    // Task categories
    void addTaskCategory(const QString& category);

    // Statistics
    int totalFamilies() const;
    int countByStatus(EffectiveContactStatus status) const;

    QJsonObject toJson() const;
    static EmergencyResponse fromJson(const QJsonObject& json);
    bool operator==(const EmergencyResponse& other) const;
    bool operator!=(const EmergencyResponse& other) const;

private:
    QString m_name;
    QDateTime m_startedAt;
    std::optional<QDateTime> m_endedAt;          // set when archived
    QHash<FamilyId, FamilyResponseRecord> m_familyRecords;
    QStringList m_taskCategories;
};
```

`create()` sets `m_startedAt` to current UTC time and populates `m_taskCategories` with defaults: `{"Tree removal", "Generator", "Medical", "Transport", "Shelter", "Other"}`.

`mutableRecord()` is **only called by `EmergencyManager`** — views use `findRecord()` (const). This keeps mutation control in the manager while avoiding excessive copying of records on every update.

Note: `EmergencyResponse::create()` does **not** take a `Document&` reference. Family records are added by `EmergencyManager::startEmergency()`, which iterates the document's families and calls `addFamilyRecord()` for each. This avoids coupling the model to `Document`.

**Step: Build and verify** — Build the app. No behavioral changes yet, just new types.

**Commit:**
```
feat: add response mode data models (ContactAttempt, ResponseTask, EmergencyResponse)
```

---

### Task 1.2: EmergencyManager Service

Create the central manager for emergency state. This is the response-data equivalent of `DocumentManager` for preparation data — but without undo/redo.

**Files:**
- Create: `src/services/EmergencyManager.h`
- Create: `src/services/EmergencyManager.cpp`
- Modify: `CMakeLists.txt` (add new source files)

**EmergencyManager class:**

```cpp
class EmergencyManager : public QObject
{
    Q_OBJECT

public:
    explicit EmergencyManager(DocumentManager* documentManager, QObject* parent);

    // Lifecycle
    bool isActive() const;
    void startEmergency(const QString& name);
    void endEmergency(bool archive);

    // Read access
    const EmergencyResponse& response() const;
    const FamilyResponseRecord* recordForFamily(const FamilyId& familyId) const;
    EffectiveContactStatus familyStatus(const FamilyId& familyId) const;

    // Contact status
    void setContactStatus(const FamilyId& familyId, ContactStatus status);

    // Contact attempts
    void addContactAttempt(const FamilyId& familyId, const ContactAttempt& attempt);
    void removeContactAttempt(const FamilyId& familyId, const ContactAttemptId& attemptId);

    // Tasks
    void addTask(const FamilyId& familyId, const ResponseTask& task);
    void updateTask(const FamilyId& familyId, const ResponseTask& task);
    void removeTask(const FamilyId& familyId, const TaskId& taskId);
    void resolveTask(const FamilyId& familyId, const TaskId& taskId, const QString& notes);

    // Task assignment
    void assignTaskToTeam(const FamilyId& familyId, const TaskId& taskId, const TeamId& teamId);
    void assignTaskToPerson(const FamilyId& familyId, const TaskId& taskId, const PersonId& personId);
    void unassignTask(const FamilyId& familyId, const TaskId& taskId);

    // Task notification
    void notifyAssignee(const FamilyId& familyId, const TaskId& taskId, const TaskNotification& notification);

    // Task categories
    QStringList taskCategories() const;
    void addTaskCategory(const QString& category);

    // Statistics
    int totalFamilies() const;
    int countByStatus(EffectiveContactStatus status) const;

signals:
    void emergencyStarted();
    void emergencyEnded();
    void familyStatusChanged(const FamilyId& familyId);
    void responseDataChanged();  // generic "something changed" for progress bars, counts

private:
    void persistResponseData();
    void syncFromDocument();
    void syncFamilies();  // add records for new families, called on document changes

    DocumentManager* m_documentManager;
    std::optional<EmergencyResponse> m_response;
};
```

**Key design decisions:**
- Every mutation method updates `m_response`, then calls `persistResponseData()` which saves to disk without disturbing command history dirty state.
- `familyStatusChanged` is emitted for targeted view updates.
- `responseDataChanged` is emitted for aggregate UI (progress bar, filter counts).
- Response data is stored in the Document's JSON under a `"responseData"` key.

**Serialization approach:** Add `std::optional<EmergencyResponse>` to `Document`. When serializing, if present, write it under `"responseData"`. On load, parse it if present.

**Step: Modify Document** — Add to `Document.h`:

```cpp
// Response data (optional — only present during active emergency)
const std::optional<EmergencyResponse>& emergencyResponse() const { return m_emergencyResponse; }
void setEmergencyResponse(const std::optional<EmergencyResponse>& response);
```

Add serialization in `Document::toJson()` and `Document::fromJson()`.

**Step: Add `setEmergencyResponse` to DocumentManager** — Add to `DocumentManager.h`:

```cpp
void setEmergencyResponse(const std::optional<EmergencyResponse>& response);
```

Implementation in `DocumentManager.cpp`:

```cpp
void DocumentManager::setEmergencyResponse(const std::optional<EmergencyResponse>& response)
{
    m_document.setEmergencyResponse(response);
    saveDocumentOnly();  // save to disk WITHOUT calling m_commandHistory.markSaved()
}
```

**New method `saveDocumentOnly()`** — saves the document to disk (using `JsonService::saveDocument`) but does **not** call `m_commandHistory.markSaved()`. This is critical: response data mutations must not reset the dirty-state tracking for preparation data. The existing `autoSave()` path calls `markSaved()`, which would incorrectly clear the undo-state awareness. `saveDocumentOnly()` bypasses that.

```cpp
bool DocumentManager::saveDocumentOnly()
{
    if (m_filePath.isEmpty())
    {
        return false;
    }
    QString errorMessage;
    if (!JsonService::saveDocument(m_filePath, m_document, &errorMessage))
    {
        qWarning() << "Response data save failed:" << errorMessage;
        emit autoSaveFailed(errorMessage);
        return false;
    }
    return true;
}
```

This preserves the const-document pattern — views read through `document()`, mutations go through dedicated methods.

**Step: Handle family additions/removals during active emergency**

Connect `DocumentManager::documentChanged` in `EmergencyManager`. On each `documentChanged` signal, compare the document's current family IDs against `m_response->familyRecords()` keys:
- **New families** (in document but not in response): create a `FamilyResponseRecord` with `NotContacted` status using the family's current `displayName()` and address.
- **Removed families** (in response but not in document): leave the record — it has snapshot data useful for the current emergency and will be included in the archive.

**Step: Load response data on document open**

In the `EmergencyManager` constructor, connect to `DocumentManager::documentChanged`. When receiving a `ChangeAction::Full` signal (indicating a new document was loaded), call `syncFromDocument()`:

```cpp
void EmergencyManager::syncFromDocument()
{
    const std::optional<EmergencyResponse>& response = m_documentManager->document().emergencyResponse();
    if (response.has_value())
    {
        m_response = response;
        syncFamilies();  // ensure any families added since last save get records
        emit emergencyStarted();
    }
    else if (m_response.has_value())
    {
        m_response.reset();
        emit emergencyEnded();
    }
}
```

This handles crash recovery and document transfer — if a document is opened that contains active `responseData`, the emergency is automatically resumed.

**Step: Build and verify**

**Commit:**
```
feat: add EmergencyManager service for response data lifecycle
```

---

### Task 1.3: Emergency Lifecycle Menu Actions

Add File → Start Emergency and File → End Emergency menu actions.

**Files:**
- Modify: `src/widgets/MainWindow.h` (add actions, slots, EmergencyManager member)
- Modify: `src/widgets/MainWindow.cpp` (menu setup, dialog implementations)

**Menu additions:**

```
File Menu:
├─ ...existing items...
├─ [separator]
├─ Start &Emergency...    → onStartEmergency()
├─ &End Emergency...      → onEndEmergency()
├─ [separator]
├─ E&xit
```

- "End Emergency" is disabled when no emergency is active
- "Start Emergency" is disabled when an emergency is already active

**Start Emergency dialog:**

Simple `QInputDialog::getText()` for the emergency name, or a small custom dialog:

```cpp
void MainWindow::onStartEmergency()
{
    bool ok;
    QString name = QInputDialog::getText(
        this,
        tr("Start Emergency"),
        tr("Emergency name:"),
        QLineEdit::Normal,
        QString(),
        &ok);

    if (!ok || name.trimmed().isEmpty())
    {
        return;
    }

    m_emergencyManager->startEmergency(name.trimmed());
}
```

**End Emergency dialog:**

Custom dialog with checkbox and three buttons:

```
┌─ End Emergency ─────────────────────┐
│                                     │
│  End "January 2026 Ice Storm"?      │
│                                     │
│  ☐ Generate summary report (PDF)    │
│                                     │
│  [Archive & End] [Discard & End] [Cancel] │
└─────────────────────────────────────┘
```

- Archive & End: calls `m_emergencyManager->endEmergency(true)`
- Discard & End: confirmation dialog ("This will permanently delete all response data"), then `m_emergencyManager->endEmergency(false)`
- PDF generation is Phase 5 — checkbox is present but non-functional until then (disabled with tooltip "Coming soon")

**Step: Connect signals to update menu state**

```cpp
connect(m_emergencyManager, &EmergencyManager::emergencyStarted, this, &MainWindow::updateEmergencyActions);
connect(m_emergencyManager, &EmergencyManager::emergencyEnded, this, &MainWindow::updateEmergencyActions);
```

**Step: Build and verify**

Start an emergency — menu items should toggle. End it (discard) — should toggle back. Check the JSON file to see responseData appear/disappear.

**Commit:**
```
feat: add Start/End Emergency menu actions and dialogs
```

---

### Task 1.4: Emergency Banner

Show a visual banner when an emergency is active.

**Files:**
- Modify: `src/widgets/MainWindow.h` (add banner widget)
- Modify: `src/widgets/MainWindow.cpp` (create and position banner)

**Banner implementation:**

Create a small `EmergencyBanner` widget class (`src/widgets/EmergencyBanner.h/cpp`) — a `QFrame` with a warning icon `QLabel` and text `QLabel`, amber background, positioned above the tab bar. Keeps banner logic out of `MainWindow` (same pattern as `EmergencyProgressBar`).

```cpp
// EmergencyBanner.h
class EmergencyBanner : public QFrame
{
    Q_OBJECT
public:
    explicit EmergencyBanner(QWidget* parent);
    void setEmergencyName(const QString& name);

private:
    QLabel* m_label;
};
```

Construction and styling go in the `.cpp`:

```cpp
EmergencyBanner::EmergencyBanner(QWidget* parent)
    : QFrame(parent)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);

    QLabel* icon = new QLabel(this);
    icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(16, 16));
    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);
    layout->addWidget(m_label, 1);

    setStyleSheet(
        "QFrame {"
        "  background-color: #FFA726;"
        "  color: #333;"
        "  font-weight: bold;"
        "}");
    hide();
}

void EmergencyBanner::setEmergencyName(const QString& name)
{
    m_label->setText(tr("EMERGENCY: %1").arg(name));
    show();
}
```

**Show/hide on emergency lifecycle:**

```cpp
void MainWindow::onEmergencyStarted()
{
    m_emergencyBanner->setEmergencyName(m_emergencyManager->response().name());
    updateEmergencyActions();
}

void MainWindow::onEmergencyEnded()
{
    m_emergencyBanner->hide();
    updateEmergencyActions();
}
```

Position the banner in the layout above the sidebar tab bar.

**Step: Build and verify**

Start an emergency — amber banner should appear with the name. End it — banner disappears.

**Commit:**
```
feat: add emergency banner with amber background
```

---

### Task 1.5: Archive Storage

Implement archive save/load for completed emergencies. Archives are full document snapshots (prep + response data), self-contained and independent of the main file. This means archived person/team names remain valid even if the ward roster changes later.

**Files:**
- Modify: `src/services/EmergencyManager.h` and `.cpp` (archive on end)

**No separate EmergencyArchive model needed.** An archive is simply a `Document` serialized with its `responseData` present — the same format as the main document file. The archive metadata (emergency name, dates) is already in `EmergencyResponse`.

(`endedAt` is already defined in `EmergencyResponse` from Task 1.1.)

**Archive directory:** Derived from document path. If document is `/path/to/MyWard.emergencyplan`, archives go in `/path/to/MyWard_archives/`.

**Archive file naming:** `YYYY-MM-DD-<emergency-name-slug>.emergencyplan`

Uses `.emergencyplan` extension since it's the same file format, loadable with the existing `JsonService::loadDocument()`.

**EmergencyManager::endEmergency(bool archive):**

```cpp
void EmergencyManager::endEmergency(bool archive)
{
    if (!m_response.has_value())
    {
        return;
    }

    if (archive)
    {
        m_response->setEndedAt(QDateTime::currentDateTimeUtc());
        // Update document with final response data before snapshotting
        m_documentManager->setEmergencyResponse(m_response);

        if (!saveArchive())
        {
            // Archive save failed — do NOT clear response data
            // User is notified by saveArchive() and can retry
            return;
        }
    }

    // Clear response data from main document
    m_response.reset();
    m_documentManager->setEmergencyResponse(std::nullopt);
    emit emergencyEnded();
}
```

**saveArchive()** — returns `true` on success, `false` on failure:

```cpp
bool EmergencyManager::saveArchive()
{
    QString docPath = m_documentManager->filePath();
    QFileInfo docInfo(docPath);
    QString archiveDir = docInfo.absolutePath() + "/" + docInfo.completeBaseName() + "_archives";

    QDir().mkpath(archiveDir);

    QString slug = m_response->name().toLower().replace(QRegularExpression("[^a-z0-9]+"), "-");
    QString date = m_response->startedAt().toString("yyyy-MM-dd");
    QString archivePath = archiveDir + "/" + date + "-" + slug + ".emergencyplan";

    // Avoid overwriting existing archive — append counter if needed
    if (QFile::exists(archivePath))
    {
        int counter = 2;
        QString basePath = archiveDir + "/" + date + "-" + slug;
        while (QFile::exists(basePath + "-" + QString::number(counter) + ".emergencyplan"))
        {
            ++counter;
        }
        archivePath = basePath + "-" + QString::number(counter) + ".emergencyplan";
    }

    // Snapshot the entire current document (prep + response)
    QString errorMessage;
    JsonService::saveDocument(archivePath, m_documentManager->document(), &errorMessage);
    if (!errorMessage.isEmpty())
    {
        qWarning() << "Archive save failed:" << errorMessage;
        QMessageBox::critical(
            nullptr,
            tr("Archive Failed"),
            tr("Could not save emergency archive:\n%1\n\nResponse data has been preserved.")
                .arg(errorMessage));
        return false;
    }
    return true;
}
```

**Step: Build and verify**

Start an emergency, end with Archive — check that a `.emergencyplan` file appears in `<DocName>_archives/`. Open it — it should contain the full document with response data. Check the main file — response data should be cleared.

**Commit:**
```
feat: archive emergency as full document snapshot on end
```

---

## Phase 2: Families View Enhancements

### Task 2.1: Status Icons on Family Rows

Add contact status icons to each family row in `WardListView`.

**Files:**
- Modify: `src/listmodels/FamilyTreeModel.h` and `.cpp` (add status role)
- Modify: `src/widgets/WardListView.cpp` (render status icons)

**Approach:** Add a custom role `ResponseStatusRole` to `FamilyTreeModel` that returns the `EffectiveContactStatus` for each family. The delegate or `data()` override renders the appropriate icon.

**Status icons:**

| Status | Icon | Color |
|--------|------|-------|
| Not contacted | No icon | — |
| OK | Checkmark ✓ | Green |
| Needs help | Flag ⚑ | Orange |
| Unable to reach | Question ? | Yellow |

Icons only appear when an emergency is active. `FamilyTreeModel` needs a pointer to `EmergencyManager` (or receives status via a method).

**Step: Connect `EmergencyManager::familyStatusChanged` to model update**

When a family's status changes, the model emits `dataChanged` for that family's row.

**Commit:**
```
feat: show contact status icons on family rows during emergency
```

---

### Task 2.2: Quick Action Buttons on Family Rows

Add [OK] [Add Task] [Unable to Reach] buttons to family rows during emergencies.

**Files:**
- Modify: `src/widgets/WardListView.cpp` (add action buttons)
- Modify: `src/widgets/WardListView.h` (add slots)

**Approach:** Similar to existing edit/delete buttons on family rows. The buttons appear on the family row, but only during an active emergency. They call through to `EmergencyManager`.

- **[OK]**: Sets status to `ContactStatus::OK`
- **[Unable to Reach]**: Sets status to `ContactStatus::UnableToReach`
- **[Add Task]**: Opens task creation dialog (Task 2.5)

**Phone number display:** Make the family's phone number more prominent in the row for quick calling. Add it as visible text (not just in the expandable section).

**Commit:**
```
feat: add emergency action buttons and phone number to family rows
```

---

### Task 2.3: Progress Bar and Status Counts

Add a segmented progress bar and filter tabs above the family list.

**Files:**
- Create: `src/widgets/EmergencyProgressBar.h` and `.cpp`
- Modify: `src/widgets/WardListView.h` and `.cpp` (add progress bar and filter tabs)

**Progress bar:**

```
47 families: 23 OK • 3 need help • 2 unable to reach • 19 remaining
[====green====][orange][yellow][----gray----]
```

A custom `QWidget` that paints colored segments proportional to status counts.

**Filter tabs:**

```
[All (47)] [Remaining (19)] [Needs Help (3)] [OK (23)] [Unable to Reach (2)]
```

A row of `QPushButton` (or `QToolButton`) styled as tabs. Clicking one filters the family list to that status. These compose with existing search/tag filters.

**Integration with Filter:** Add `EffectiveContactStatus` filter to `Filter.h`:

```cpp
void setContactStatusFilter(std::optional<EffectiveContactStatus> status);
```

The filter passes families based on their response status when set.

**Live updates:** Connect `EmergencyManager::responseDataChanged` to rebuild counts and progress bar.

**Commit:**
```
feat: add emergency progress bar and status filter tabs
```

---

### Task 2.4: Contact Attempt History

Show contact attempts in the expandable family section and provide an "Add Contact Attempt" dialog.

**Files:**
- Modify: `src/listmodels/FamilyTreeModel.h` and `.cpp` (add contact attempt rows)
- Modify: `src/widgets/WardListView.cpp` (add contact attempt dialog)

**Expandable section additions:**

```
Contact Attempts:
  Phone - John Smith - Jan 4, 2:15 PM - "Left voicemail"
  Text - Jane Doe - Jan 4, 3:30 PM
```

Each attempt shows: method, who (person name), timestamp, notes (if any).

**Add Contact Attempt dialog:**

```
┌─ Log Contact Attempt ──────────────┐
│                                     │
│  Method: ○ Phone ○ Text ○ Email    │
│          ○ Visit ○ Other            │
│                                     │
│  Who: [Dropdown of ward members]    │
│                                     │
│  Notes: [_________________________] │
│                                     │
│  [Save] [Cancel]                    │
└─────────────────────────────────────┘
```

- Method: radio buttons
- Who: combo box of all persons in the document (searchable)
- Notes: optional text field (required if method is Other)
- Timestamp: auto-set to current time

**Commit:**
```
feat: add contact attempt history and logging dialog
```

---

### Task 2.5: Task Management

Implement task creation, editing, assignment, notification, and resolution.

**Files:**
- Modify: `src/widgets/WardListView.cpp` (task display in expandable section, task dialogs)
- Modify: `src/widgets/WardListView.h`

**Expandable section — tasks display:**

```
Tasks:
  • Generator - "No power since Tuesday"
    Assigned to Chainsaw Crew ✓ notified
    [Edit] [Resolve]
  • Medical - "Needs medication pickup"
    Assigned to Dr. Smith ○ not notified
    [Edit] [Notify] [Resolve]
```

**Add/Edit Task dialog:**

```
┌─ Add Task ─────────────────────────┐
│                                     │
│  Category: [Dropdown ▼]            │
│            + Add new...             │
│                                     │
│  Description: [____________________]│
│                                     │
│  Assign to:                         │
│  ○ Unassigned                       │
│  ○ Team: [Dropdown ▼]              │
│  ○ Person: [Dropdown ▼]            │
│                                     │
│  [Save] [Cancel]                    │
└─────────────────────────────────────┘
```

- Category: dropdown from configured list, plus "Add new..." option that prompts for a name and adds to the list
- Assign: optional, radio buttons for team or person

**Notify dialog** (quick picker):

```
┌─ Notify Assignee ──────────────────┐
│                                     │
│  Method: ○ Phone ○ Text ○ Email    │
│          ○ Visit ○ Other            │
│                                     │
│  Notes: [_________________________] │
│                                     │
│  [Notify] [Cancel]                  │
└─────────────────────────────────────┘
```

Records timestamp automatically.

**Resolve** — clicking [Resolve] either:
- Quick resolve (no dialog if no notes needed)
- Or shows a small dialog for resolution notes

When all tasks for a family are resolved, `effectiveStatus()` naturally changes from `NeedsHelp` back to the stored `contactStatus` (OK or UnableToReach), since NeedsHelp is a derived status — no explicit status reset is needed.

**Task categories** — stored in `EmergencyResponse::taskCategories`. New categories added during an emergency persist in the response data. On next emergency, the default list is used again (emergency-scoped, not persisted to settings).

**Commit:**
```
feat: add task creation, assignment, notification, and resolution
```

---

### Task 2.6: Map Marker Badges

Add welfare check status badges to family markers on the map.

**Files:**
- Modify: `src/widgets/MapWidget.cpp` (draw badges on markers)
- Modify: `src/widgets/MapWidget.h` (add EmergencyManager pointer)

**Badge rendering:**

Small icon in the bottom-right corner of each family marker:

| Status | Badge |
|--------|-------|
| Not contacted | No badge |
| OK | Small green checkmark |
| Needs help | Small orange flag |
| Unable to reach | Small yellow question mark |

Badges only render when an emergency is active and only for families whose status has changed from NotContacted.

**Approach:** In `MapWidget::drawMarkers()`, after drawing the main marker, check the emergency manager for status and draw the badge overlay. The badge is small (~8x8 pixels) positioned at the bottom-right of the marker bounds.

**Existing ResponseArea colors are preserved.** The badge is additive, not replacing existing marker colors.

**Commit:**
```
feat: add welfare check status badges to map markers
```

---

## Phase 3: Ministering View Enhancements

### Task 3.1: Status Icons in Ministering Tree

Add contact status icons to families in the ministering hierarchy.

**Files:**
- Modify: `src/listmodels/MinisteringModel.h` and `.cpp`
- Modify: `src/widgets/MinisteringView.cpp` (or `MinisteringTabView`)

**Per family in the tree (both EQ and RS):**
- Status icon (same as Families view: ✓/⚑/?/none)
- RS assigns ministers to sisters, but response status is per-family. A ministered sister's status is her family's status. Update the family's response record when status is changed from either view.

**Per companionship:**
- Compact status summary: `[✓✓✓○]` showing icons for each assigned family

**Per district:**
- Progress bar segment (same style as Families view progress bar) next to district name

**Contact info:**
- District leader phone number shown next to district name
- Minister phone numbers shown next to minister names

**Commit:**
```
feat: add emergency status icons and progress to ministering view
```

---

## Phase 4: Teams View Enhancements

### Task 4.1: Task List Per Team

Show assigned tasks under each team during emergencies.

**Files:**
- Modify: `src/listmodels/TeamListModel.h` and `.cpp` (or create new tree model)
- Modify: `src/widgets/` (teams view — find existing view)

**Display:**

```
Chainsaw Crew (4 members)
├── Generator - Anderson Family - "No power since Tuesday" ✓ notified
├── Tree removal - Baker Family - "Tree on driveway" ○ not notified
└── Tree removal - Clark Family - "Blocking road" ✓ notified [RESOLVED]
```

Each task row shows: category, family name, description, notification status, resolution status.

**Unassigned tasks section** at top or bottom:

```
Unassigned Tasks
├── Medical - Davis Family - "Needs medication pickup"
└── Shelter - Evans Family - "Roof damage"
```

**Task actions from Teams view:**
- [Assign] — assign an unassigned task to this team
- [Notify] — record that the assignee was notified
- [Resolve] — mark task as resolved

**Commit:**
```
feat: show assigned tasks per team during emergency
```

---

## Phase 5: Summary Reports

### Task 5.1: Summary Report Generation

Generate a PDF summary report of the emergency response.

**Files:**
- Create: `src/services/ReportGenerator.h` and `.cpp`

**Report content** (as specified in design doc):

1. **Header:** Emergency name, date range, ward name
2. **Overview statistics:** Total families, contacted count, contact rate
3. **Status breakdown:** OK/NeedsHelp/UnableToReach/NotContacted counts and percentages
4. **Task summary:** By category, resolved vs unresolved, assigned vs unassigned
5. **Response activity:** Total contact attempts, by method, tasks assigned, resolution rate
6. **Unresolved items:** Families still needing help, families not contacted

**PDF generation:** Use `QPrinter` with `QPainter`. The report is straightforward text and tables, so QPrinter is the right tool. (MuPDF is a reading library — its PDF creation API is low-level and not suitable here.)

**Trigger:** The "Generate summary report" checkbox in the End Emergency dialog. Also available via a menu action during an active emergency (File → Generate Emergency Report).

**Commit:**
```
feat: generate PDF summary report for emergency response
```

---

## Phase 6: Archives

### Task 6.1: Archive Browser

Add File → Open Emergency Archive menu action and browser dialog.

**Files:**
- Create: `src/widgets/ArchiveBrowserDialog.h` and `.cpp`
- Modify: `src/widgets/MainWindow.h` and `.cpp` (menu action)

**Archive browser dialog:**

```
┌─ Emergency Archives ──────────────────────────┐
│                                                │
│  January 2026 Ice Storm - Jan 4-6, 2026       │
│    47 families, 12 tasks                       │
│                                                │
│  December 2025 Power Outage - Dec 18, 2025    │
│    47 families, 3 tasks                        │
│                                                │
│  [View] [Reopen] [Delete] [Close]              │
└────────────────────────────────────────────────┘
```

Reads archive files from the `<DocumentName>_archives/` directory. Lists them sorted by date (most recent first).

**Commit:**
```
feat: add emergency archive browser dialog
```

---

### Task 6.2: Read-Only Archive Viewing

Open an archive in read-only mode overlaid on the current views.

**Files:**
- Modify: `src/services/EmergencyManager.h` and `.cpp` (add archive viewing mode)
- Modify: `src/widgets/MainWindow.cpp` (banner and view state)
- Modify views as needed for read-only display

**Archive viewing mode:**

Since archives are full document snapshots, viewing is straightforward:
- Load the archive with `JsonService::loadDocument()` into a separate `Document` instance
- `EmergencyManager` enters a read-only archive mode with the loaded document's response data
- Views read from the archive's document for family/person/team names (frozen at archive time), not the current document
- Banner shows: `"ARCHIVED - READ ONLY: January 2026 Ice Storm"`
- All action buttons are hidden/disabled
- No need for family matching — the archive has its own complete family data

**Close archive:** Menu action or banner button returns to normal view (current document).

**Commit:**
```
feat: view archived emergencies in read-only mode
```

---

### Task 6.3: Reopen Archived Emergency

Allow reopening an archived emergency as active.

**Files:**
- Modify: `src/services/EmergencyManager.h` and `.cpp`
- Modify: `src/widgets/ArchiveBrowserDialog.cpp`

**Reopen flow:**

1. Confirm: "This will restore this emergency as active. Any current emergency will be ended."
2. If a current emergency is active, end it (with archive prompt)
3. Load the archived response data as the active emergency
4. Delete the archive file (it's now the active response)

**Commit:**
```
feat: allow reopening archived emergencies
```

---

## Summary of New Files

| File | Purpose |
|------|---------|
| `src/models/EmergencyResponse.h/cpp` | Response data models (ContactAttempt, ResponseTask, FamilyResponseRecord, EmergencyResponse) |
| `src/services/EmergencyManager.h/cpp` | Response data lifecycle and mutation methods |
| `src/services/ReportGenerator.h/cpp` | PDF summary report generation |
| `src/widgets/EmergencyBanner.h/cpp` | Amber emergency banner widget |
| `src/widgets/EmergencyProgressBar.h/cpp` | Segmented progress bar widget |
| `src/widgets/ArchiveBrowserDialog.h/cpp` | Archive list and actions dialog |

Note: Each task that creates new files implicitly updates `CMakeLists.txt`.

## Modified Files

| File | Change |
|------|--------|
| `src/models/Id.h` | Add `TaskId`, `ContactAttemptId`, `Q_DECLARE_METATYPE` |
| `src/models/Document.h/cpp` | Add optional `EmergencyResponse`, serialization |
| `src/services/DocumentManager.h/cpp` | Add `setEmergencyResponse()` and `saveDocumentOnly()` methods |
| `src/widgets/MainWindow.h/cpp` | Menu actions, banner, EmergencyManager wiring |
| `src/widgets/WardListView.h/cpp` | Status icons, action buttons, progress bar, filter tabs, contact/task dialogs |
| `src/widgets/MapWidget.h/cpp` | Status badges on markers |
| `src/listmodels/FamilyTreeModel.h/cpp` | Response status role, contact/task rows |
| `src/listmodels/MinisteringModel.h/cpp` | Status icons, progress per district/companionship (EQ and RS) |
| `src/listmodels/Filter.h/cpp` | Contact status filter |

## Prerequisites

- **Teams view**: Implemented (was `PlaceholderView`, now complete). Phase 4 is unblocked.

## Resolved Items

- **Map dimming**: Already implemented in current codebase. No changes needed.
- **Task categories**: Emergency-scoped (fresh defaults each time). Simpler than Settings configuration and avoids cross-emergency state.
- **RS ministering**: Sister status == family status. Show and update family response records from both EQ and RS views.
- **Banner icon**: Uses `QStyle::SP_MessageBoxWarning` instead of Unicode emoji for cross-platform compatibility.

## Review Decisions Applied

- **No undo/redo** for response actions (issue #1)
- **Task assignee**: `std::optional<TeamId>` + `std::optional<PersonId>`, at most one (issue #2)
- **Contact attempt `who`**: `PersonId` only, ward members (issue #3)
- **Auto-save**: already implemented for all data (issue #4)
- **Notification**: single record per task (issue #5)
- **Archives**: `<DocumentName>_archives/` directory, full document snapshots (issue #6)
- **Banner/sidebar layout**: confirmed existing layout is compatible (issue #7)
- **Map badges**: additive, bottom-right corner (issue #8)
- **Task categories**: emergency-scoped, default list on each new emergency (minor note #2)
- **Archive matching**: not needed — archives contain full document snapshot (minor note #3)

## Review Fixes Applied (2026-03-09)

- **C1: Model encapsulation** — All model types converted from `struct`/public-members to `class` with private `m_` members, getters, setters, and factory `create()` methods, matching `Team`/`Family`/`Person` pattern
- **C2: Mutable reference removed** — `EmergencyResponse::recordForFamily()` replaced with `mutableRecord()` (called only by EmergencyManager) and const `findRecord()` (for views)
- **C3: Consistent parameter passing** — `Document::setEmergencyResponse` and `DocumentManager::setEmergencyResponse` both use `const std::optional<EmergencyResponse>&`
- **C4: Dirty-state preservation** — Added `DocumentManager::saveDocumentOnly()` that writes to disk without calling `m_commandHistory.markSaved()`, preventing response mutations from resetting preparation-data dirty tracking
- **I1: Default parameter removed** — `EmergencyManager` constructor requires explicit `QObject* parent`
- **I2: Family sync specified** — Explicit algorithm: compare document family IDs against response record keys on each `documentChanged` signal
- **I3: Snapshot timing specified** — `displayName`/`address` set once at emergency start, never updated during the emergency
- **I4: Document open loading** — `syncFromDocument()` handles `ChangeAction::Full` to auto-resume emergencies from saved response data (crash recovery, machine transfer)
- **I5: Archive filename collisions** — Counter-based deduplication when archive file already exists
- **I6: EmergencyResponse::create() decoupled from Document** — `create()` takes only the name; `EmergencyManager::startEmergency()` iterates families and calls `addFamilyRecord()`
- **I7: NeedsHelp clarification** — Explicitly documented that NeedsHelp is derived and resolving all tasks naturally changes `effectiveStatus()` back to stored `contactStatus`
- **I8: Archive failure handling** — `saveArchive()` returns bool; on failure, shows `QMessageBox::critical` and does NOT clear response data
- **M1: Banner extracted to widget** — `EmergencyBanner` class in `src/widgets/EmergencyBanner.h/cpp`, consistent with `EmergencyProgressBar`
- **M2: MuPDF mention removed** — Phase 5 now specifies QPrinter/QPainter only
- **M3: endedAt in Task 1.1** — Field included in initial `EmergencyResponse` model definition
- **S3: Banner text uses tr()** — `tr("EMERGENCY: %1").arg(name)` for localization

## Review Fixes Applied (2026-03-10)

- **S-NEW-3: ID type constructor accessibility** — Moved `using IdBase::IdBase;` before `public:` in `TaskId` and `ContactAttemptId` to match existing ID types (constructors stay private, only `generate()`/`fromString()` are public)
- **I-NEW-1/2: `saveDocumentOnly()` error handling** — Now returns `bool`, checks `JsonService::saveDocument` return value (not just error message), and emits `autoSaveFailed` on failure so MainWindow can notify the user
- **I-NEW-3: `syncFromDocument()` calls `syncFamilies()`** — On document open with active response data, ensures any families added since last save get response records
