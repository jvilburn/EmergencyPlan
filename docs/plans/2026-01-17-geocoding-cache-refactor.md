# Geocoding Cache Refactor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace `familyWithChangedAddress()` hook with a reactive cache-based geocoding system that responds to DocumentChange signals.

**Architecture:** BackgroundGeocodingService listens to DocumentChange, maintains an address→coords cache and per-family address tracking, and automatically queues families for geocoding when needed. Cache is seeded on document load; subsequent family changes check if address changed to decide whether to re-geocode or trust existing coords.

**Tech Stack:** Qt 6.10, C++17, MSVC 2022

**Design Document:** [2026-01-11-document-change-design.md](2026-01-11-document-change-design.md)

---

## Background

### Current Problem
The `familyWithChangedAddress()` virtual method on Command is a code smell - a special-case hook that only exists for geocoding, violating separation of concerns.

### New Approach
BackgroundGeocodingService becomes reactive:
- Listens to `documentChanged` signal
- Tracks each family's last known address (to detect changes)
- Maintains cache: `address → coords` (for efficiency)
- On document load: seed both caches, queue unmapped families
- On family change: if address changed → re-geocode; if only coords changed → trust user correction

### Key Logic
- **Address changed** → existing coords are stale, use cache or API for new address
- **Address unchanged + mapped** → trust existing coords (user correction or already geocoded)
- **Address unchanged + unmapped** → use cache or queue API

---

### Task 1: Update BackgroundGeocodingService header

**Files:**
- Modify: `src/services/BackgroundGeocodingService.h`

**Step 1: Add forward declaration and includes**

At top of file, after existing includes, add:
```cpp
#include "DocumentChange.h"
#include <QPointF>

class DocumentManager;
```

**Step 2: Change constructor signature**

Change from:
```cpp
    explicit BackgroundGeocodingService(QObject* parent = nullptr);
```

To:
```cpp
    explicit BackgroundGeocodingService(DocumentManager* documentManager);
```

**Step 3: Add onDocumentChanged slot**

Change the private slots section to public slots and add the new slot:
```cpp
public slots:
    /// Handle document changes - seeds cache or queues families as needed.
    void onDocumentChanged(const DocumentChange& change);

private slots:
    void onGeocodingComplete(const GeocodingResult& result);
```

**Step 4: Add private members and helpers**

In private section, add:
```cpp
    DocumentManager* m_documentManager;

    /// Cache: address string → coordinates.
    /// Seeded from document on load, populated by API responses.
    QHash<QString, QPointF> m_geocodeCache;

    /// Tracks each family's last known address for change detection.
    QHash<QString, QString> m_familyAddresses;

    /// Process all families on document load or batch change.
    void processAllFamilies();

    /// Check a single family against cache, queue or apply coords as needed.
    void checkFamily(const Family& family);
```

**Step 5: Run build**

Run: `build.bat`
Expected: Compilation errors in .cpp file (constructor signature changed, methods not implemented)

---

### Task 2: Update BackgroundGeocodingService constructor and add signal connection

**Files:**
- Modify: `src/services/BackgroundGeocodingService.cpp`

**Step 1: Add includes at top**

```cpp
#include "DocumentManager.h"
#include "DocumentChange.h"
```

**Step 2: Update constructor**

Change from:
```cpp
BackgroundGeocodingService::BackgroundGeocodingService(QObject* parent)
    : QObject(parent)
    , m_geocodingService(new GeocodingService(this))
{
    connect(m_geocodingService, &GeocodingService::geocodingComplete,
            this, &BackgroundGeocodingService::onGeocodingComplete);
}
```

To:
```cpp
BackgroundGeocodingService::BackgroundGeocodingService(DocumentManager* documentManager)
    : QObject(documentManager)
    , m_documentManager(documentManager)
    , m_geocodingService(new GeocodingService(this))
{
    connect(m_geocodingService, &GeocodingService::geocodingComplete,
            this, &BackgroundGeocodingService::onGeocodingComplete);

    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &BackgroundGeocodingService::onDocumentChanged);
}
```

**Step 3: Run build**

Run: `build.bat`
Expected: Errors for missing onDocumentChanged, processAllFamilies, checkFamily implementations

---

### Task 3: Implement onDocumentChanged

**Files:**
- Modify: `src/services/BackgroundGeocodingService.cpp`

**Step 1: Add onDocumentChanged implementation**

Add after constructor:
```cpp
void BackgroundGeocodingService::onDocumentChanged(const DocumentChange& change)
{
    // Full document change or batch family change - process all families
    if (change.scope == ChangeScope::Full
        || (change.scope == ChangeScope::Family
            && change.action == ChangeAction::BatchModified))
    {
        processAllFamilies();
        return;
    }

    // Single family change - check that family
    if (change.scope == ChangeScope::Family)
    {
        auto family = m_documentManager->document().findFamilyById(change.entityId);
        if (family.has_value())
        {
            checkFamily(*family);
        }
    }
}
```

**Step 2: Run build**

Run: `build.bat`
Expected: Errors for missing processAllFamilies, checkFamily implementations

---

### Task 4: Implement checkFamily

**Files:**
- Modify: `src/services/BackgroundGeocodingService.cpp`

**Step 1: Add checkFamily implementation**

Add after onDocumentChanged:
```cpp
void BackgroundGeocodingService::checkFamily(const Family& family)
{
    QString address = family.address().full();
    QString lastAddress = m_familyAddresses.value(family.id());

    bool addressChanged = (address != lastAddress);

    // Update address tracking
    if (!address.isEmpty())
    {
        m_familyAddresses.insert(family.id(), address);
    }
    else
    {
        m_familyAddresses.remove(family.id());
    }

    if (address.isEmpty())
    {
        return;
    }

    if (addressChanged)
    {
        // Address changed - existing coords are stale, need fresh geocoding
        if (m_geocodeCache.contains(address))
        {
            QPointF cached = m_geocodeCache.value(address);
            emit familyGeocoded(family.id(), cached.x(), cached.y());
        }
        else
        {
            queueFamily(family);
        }
        return;
    }

    // Address unchanged
    if (family.isMapped())
    {
        // Has coords for current address - trust them (user correction or already geocoded)
        return;
    }

    // Unmapped - apply cache if available, otherwise queue
    if (m_geocodeCache.contains(address))
    {
        QPointF cached = m_geocodeCache.value(address);
        emit familyGeocoded(family.id(), cached.x(), cached.y());
    }
    else
    {
        queueFamily(family);
    }
}
```

**Step 2: Run build**

Run: `build.bat`
Expected: Error for missing processAllFamilies implementation

---

### Task 5: Implement processAllFamilies

**Files:**
- Modify: `src/services/BackgroundGeocodingService.cpp`

**Step 1: Add processAllFamilies implementation**

Add after checkFamily:
```cpp
void BackgroundGeocodingService::processAllFamilies()
{
    m_geocodeCache.clear();
    m_familyAddresses.clear();
    const auto& families = m_documentManager->document().families();

    // First pass: seed caches from document state.
    // - Address tracking: so we can detect changes later
    // - Coord cache: so families at same address can share coords
    for (const auto& family : families)
    {
        QString address = family.address().full();

        if (!address.isEmpty())
        {
            m_familyAddresses.insert(family.id(), address);

            if (family.isMapped())
            {
                m_geocodeCache.insert(address,
                    QPointF(family.latitude().value(), family.longitude().value()));
            }
        }
    }

    // Second pass: check each family against the now-seeded cache.
    // Since addresses are already tracked, checkFamily will see addressChanged=false,
    // trusting mapped families and only geocoding unmapped ones.
    for (const auto& family : families)
    {
        checkFamily(family);
    }
}
```

**Step 2: Run build**

Run: `build.bat`
Expected: BUILD SUCCESS

---

### Task 6: Update onGeocodingComplete to populate cache

**Files:**
- Modify: `src/services/BackgroundGeocodingService.cpp`

**Step 1: Add cache population in onGeocodingComplete**

Find the existing `onGeocodingComplete` method. After the success check and before emitting `familyGeocoded`, add cache population.

Change from:
```cpp
    if (result.success && result.latitude.has_value() && result.longitude.has_value())
    {
        emit familyGeocoded(familyId, *result.latitude, *result.longitude);
    }
```

To:
```cpp
    if (result.success && result.latitude.has_value() && result.longitude.has_value())
    {
        // Add to cache for future lookups
        m_geocodeCache.insert(result.address,
            QPointF(*result.latitude, *result.longitude));

        emit familyGeocoded(familyId, *result.latitude, *result.longitude);
    }
```

**Step 2: Run build**

Run: `build.bat`
Expected: BUILD SUCCESS

---

### Task 7: Update DocumentManager to use new constructor

**Files:**
- Modify: `src/services/DocumentManager.cpp`

**Step 1: Verify construction compiles**

The constructor already passes `this` as parent. Verify it compiles since BackgroundGeocodingService now expects `DocumentManager*` instead of `QObject*`.

**Step 2: Run build**

Run: `build.bat`
Expected: BUILD SUCCESS

---

### Task 8: Remove familyWithChangedAddress usage from DocumentManager

**Files:**
- Modify: `src/services/DocumentManager.cpp`

**Step 1: Simplify executeCommand**

Change from:
```cpp
void DocumentManager::executeCommand(CommandPtr command)
{
    // Check before moving - trigger geocoding if address changed
    QString familyId = command->familyWithChangedAddress();
    DocumentChange change = command->documentChange();

    m_commandHistory.execute(std::move(command), m_document);
    emit documentChanged(change);
    checkForIncompleteWards();

    if (!familyId.isEmpty())
    {
        queueFamilyForGeocoding(familyId);
    }
}
```

To:
```cpp
void DocumentManager::executeCommand(CommandPtr command)
{
    DocumentChange change = command->documentChange();

    m_commandHistory.execute(std::move(command), m_document);
    emit documentChanged(change);
    checkForIncompleteWards();
}
```

**Step 2: Run build**

Run: `build.bat`
Expected: BUILD SUCCESS

---

### Task 9: Remove queueFamilyForGeocoding from DocumentManager

**Files:**
- Modify: `src/services/DocumentManager.h`
- Modify: `src/services/DocumentManager.cpp`

**Step 1: Remove declaration from header**

In `DocumentManager.h`, remove:
```cpp
    void queueFamilyForGeocoding(const QString& familyId);
```

**Step 2: Remove implementation from cpp**

In `DocumentManager.cpp`, remove the entire method:
```cpp
void DocumentManager::queueFamilyForGeocoding(const QString& familyId)
{
    auto family = m_document.findFamilyById(familyId);
    if (family.has_value() && !family->address().isEmpty())
    {
        m_geocodingService->queueFamily(*family);
    }
}
```

**Step 3: Run build**

Run: `build.bat`
Expected: BUILD SUCCESS

---

### Task 10: Remove familyWithChangedAddress from FamilyCommands

**Files:**
- Modify: `src/commands/FamilyCommands.h`
- Modify: `src/commands/FamilyCommands.cpp`

**Step 1: Remove declaration from header**

In `FamilyCommands.h`, find the UpdateFamilyCommand class and remove:
```cpp
    QString familyWithChangedAddress() const override;
```

**Step 2: Remove implementation from cpp**

In `FamilyCommands.cpp`, remove the entire method:
```cpp
QString UpdateFamilyCommand::familyWithChangedAddress() const
{
    if (m_oldFamily.address() != m_newFamily.address())
    {
        return m_newFamily.id();
    }
    return {};
}
```

**Step 3: Run build**

Run: `build.bat`
Expected: BUILD SUCCESS

---

### Task 11: Remove familyWithChangedAddress from Command base class

**Files:**
- Modify: `src/commands/Command.h`

**Step 1: Remove the virtual method**

Remove:
```cpp
    // Returns ID of family whose address was changed by this command.
    // Empty string if no address changed. Used to trigger background geocoding.
    virtual QString familyWithChangedAddress() const { return {}; }
```

**Step 2: Run build**

Run: `build.bat`
Expected: BUILD SUCCESS

---

### Task 12: Update CLAUDE.md to remove technical debt note

**Files:**
- Modify: `CLAUDE.md`

**Step 1: Remove the technical debt section**

Find and remove the entire "High Priority Technical Debt" section:
```markdown
## High Priority Technical Debt

### Command::familyWithChangedAddress() - BAD DESIGN
The `familyWithChangedAddress()` virtual method on Command is a code smell. It's a special-case hook that only exists for geocoding, violating separation of concerns. When we implement `DocumentChange` (see docs/plans), this should be refactored so that:
- Commands return a `DocumentChange` describing what changed
- Geocoding logic observes family changes and checks if address field differs
- No command-specific hooks for individual side effects
```

**Step 2: Verify file is well-formed**

Read the file to ensure no orphaned references remain.

---

### Task 13: Commit the changes

**Step 1: Stage all changes**

```bash
git add -A
```

**Step 2: Review changes**

```bash
git diff --cached --stat
```

Expected files:
- `src/services/BackgroundGeocodingService.h`
- `src/services/BackgroundGeocodingService.cpp`
- `src/services/DocumentManager.h`
- `src/services/DocumentManager.cpp`
- `src/commands/Command.h`
- `src/commands/FamilyCommands.h`
- `src/commands/FamilyCommands.cpp`
- `CLAUDE.md`

**Step 3: Commit**

```bash
git commit -m "$(cat <<'EOF'
refactor: replace familyWithChangedAddress with reactive geocoding cache

- BackgroundGeocodingService now listens to documentChanged signal
- Tracks per-family addresses to detect address changes
- Maintains address→coords cache for efficiency and undo/redo support
- Address changed → re-geocode (stale coords ignored)
- Address unchanged + mapped → trust user corrections
- Address unchanged + unmapped → use cache or queue API
- Removes familyWithChangedAddress() from Command (technical debt resolved)
- Removes queueFamilyForGeocoding() from DocumentManager

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 14: Manual verification

**Step 1: Launch the application**

Run: `build\WardPlanning.exe`

**Step 2: Test document load**

1. Open an existing ward document with families
2. Verify families with addresses appear on the map
3. Verify geocoding progress shows for unmapped families

**Step 3: Test family address editing**

1. Select a family and change its address
2. Verify geocoding starts for the new address
3. Verify the family marker moves on the map when geocoding completes

**Step 4: Test manual coord correction**

1. Select a family with geocoded coords
2. Manually edit the coords (without changing address)
3. Verify the coords are NOT overwritten by cache
4. Edit some other field on the family
5. Verify the manual coords are still preserved

**Step 5: Test undo/redo**

1. After changing an address, press Ctrl+Z to undo
2. Verify the old address is restored
3. Verify the old coordinates are restored (from cache, no API call)
4. Press Ctrl+Y to redo
5. Verify the new address and coordinates return

**Step 6: Test duplicate addresses**

1. Edit a family to have the same address as another family
2. Verify coordinates are applied immediately (from cache)
3. Verify no geocoding API call is made

---

## Summary

This refactor:
1. Removes the `familyWithChangedAddress()` technical debt from Command
2. Makes BackgroundGeocodingService reactive (responds to DocumentChange)
3. Tracks per-family addresses to detect address changes
4. Adds address→coords cache for efficiency and undo/redo support
5. Preserves user manual coord corrections
6. Simplifies DocumentManager by removing geocoding orchestration logic
