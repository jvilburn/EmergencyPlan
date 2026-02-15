# Design Review: Strong ID Types

**Design document:** `docs/plans/2026-02-11-strong-id-types-design.md`
**Reviewed:** 2026-02-12
**Reviewer:** Claude Opus 4.6 (code review agent)

---

## Summary

The design is thorough, architecturally sound, and covers the vast majority of QString ID usages across the codebase. The CRTP approach, DocumentChange redesign, and Tag split are all well-justified. The main gap is that roughly 8--10 files in the services, viewmodels, and commands layers are not mentioned in the impact tables, and the `SelectionKey::from` template uses a C++20 concept in a C++17 project.

---

## Strengths

- **Comprehensive core coverage.** The design covers models, commands, list models, widgets, Filter, SelectionPreservingTreeView, and BaseTreeModel -- essentially every layer of the application. The impact tables are detailed and precise.

- **Sound CRTP design.** The base template correctly prevents cross-type comparison at compile time, provides `qHash` for QHash usage, `operator<` for ordered containers, and a debug stream operator. The rationale for CRTP over alternatives is convincing and well-explained.

- **Clean `std::optional` semantics.** Replacing empty-string-as-null with `std::optional<PersonId>` for optional IDs is a genuine correctness improvement. The debug-only `Q_ASSERT` on `toString()` is a pragmatic safeguard. The note about fixing the pre-existing `std::optional<QString>` violations in MinisteringGroup and MinisteringDistrict is a nice touch.

- **DocumentChange redesign.** Replacing `ChangeScope` + polymorphic `QString entityId` with typed optional fields is a strict improvement. The consumer pattern shown (checking `change.familyId` instead of `scope == ChangeScope::Family`) is clearer and type-safe. Merging `BatchModified` into `Full` is well-justified.

- **SelectionKey as an opaque type.** This correctly models the semantic distinction between entity IDs and selection keys, while still allowing composite keys.

- **Tag entityIds split.** Splitting `QSet<QString> m_entityIds` into `QSet<PersonId>` + `QSet<FamilyId>` eliminates a type-safety hole and mirrors the existing MinisteringGroup pattern.

- **JSON compatibility preserved.** No format changes needed -- IDs are still strings in JSON. The boundary conversion via `fromString`/`toString` is clean.

---

## Issues

### Critical (Must Fix)

**1. ~~FIXED~~ `SelectionKey::from` uses C++20 `std::derived_from` concept, but the project is C++17.**

- **Location in design:** Line 162 (`requires std::derived_from<Id, IdBase<Id>>`)
- **Location in source:** `CMakeLists.txt` line 12 (`set(CMAKE_CXX_STANDARD 17)`), `CMakePresets.json` line 14 (`"CMAKE_CXX_STANDARD": "17"`)
- **What's wrong:** The `requires` clause and `std::derived_from` are C++20 features. The project explicitly sets C++17 in both CMakeLists.txt and CMakePresets.json.
- **Why it matters:** This will not compile.
- **How to fix:** Replace with a SFINAE constraint:

```cpp
template<typename Id, std::enable_if_t<std::is_base_of_v<IdBase<Id>, Id>, int> = 0>
static SelectionKey from(const Id& id) { return SelectionKey(id.toString()); }
```

Or, since the constructor is private and `from` is the only template entry point, a simpler option is to drop the constraint entirely and rely on the fact that `id.toString()` will only compile for types that have it. The CRTP base provides `toString()`, so any non-IdBase type would fail at `id.toString()`. The constraint is nice-to-have documentation but not strictly necessary for correctness. If the team prefers the constraint, use the SFINAE version above.

---

### Important (Should Fix)

**2. ~~FIXED~~ Multiple services files with entity IDs are missing from the impact tables.**

- **Location in design:** The Impact Summary (lines 311--540) has no "Services" or "ViewModels" section.
- **Source files affected:**
  - `src/services/BackgroundGeocodingService.h` lines 52, 73, 76, 79 -- signal `familyGeocoded(const QString& id, ...)`, `m_familyAddresses` (`QHash<QString, QString>` keyed by family ID), `m_addressToFamilyId` (`QHash<QString, QString>` with family ID values), `m_queuedIds` (`QSet<QString>` of family IDs)
  - `src/services/DocumentManager.h` line 72 -- slot `onFamilyGeocoded(const QString& id, ...)` is a family ID
  - `src/services/PersonMatching.h` lines 23, 28--29, 32, 38--41 -- `PersonMatchScore::personId` (person ID), `FamilyMemberMatchResult::familyId` (family ID), `FamilyMemberMatchResult::personIdMapping` (`QHash<QString, QString>` person-to-person), `FamilyReplacementResult::replacedFamilyId` (family ID), `FamilyReplacementResult::personIdMapping` (`QHash<QString, QString>` person-to-person)
  - `src/services/MinisteringImportService.h` line 82 -- `mergeFamilyMembers(..., QHash<QString, QString>& personIdMapping, ...)`, and `.cpp` local variables `familyIdMapping`, `personIdMapping`
  - `src/services/WardDirectoryImportService.h` lines 18--19 -- `WardDirectoryImportResult::families` (`QHash<QString, Family>` keyed by family ID), `WardDirectoryImportResult::removedFamilyIds` (`QSet<QString>` of family IDs)
- **What's wrong:** These files contain entity IDs that are in-scope for the refactor but are not listed anywhere in the design.
- **Why it matters:** An implementer following only the design document would miss these files, leading to compile errors at boundaries between typed and untyped code. The import services are particularly important since they construct IDs during PDF import.
- **How to fix:** Add a "Services" section to the Impact Summary covering these files. Note that `DocumentManager::m_pendingWardLookups` and `m_pendingStakeLookups` (`QSet<QString>`) are ward/stake unit numbers and correctly remain `QString`.

---

**3. ~~FIXED~~ `MapViewModel` has extensive family ID usage not covered by the design.**

- **Location in design:** Not mentioned anywhere.
- **Source file:** `src/viewmodels/MapViewModel.h` lines 33, 45, 52, 55--56, 65, 76, 91--92, 97
- **What's wrong:** `MapViewModel` contains:
  - `selectedFamilyId()` / `setSelectedFamilyId(const QString&)` -- family IDs
  - `selectFamily(const QString& id)` / `centerOnFamily(const QString& id)` -- family IDs
  - `familyIcons(const QString& familyId)` -- family ID parameter
  - `familyClicked(const QString& id)` signal -- family ID
  - `m_familyIcons` (`QHash<QString, MarkerIcons>`) -- keyed by family ID
  - `m_selectedId` (`QString`) -- family ID
- **Why it matters:** MapViewModel is a major consumer of family IDs. Omitting it creates an incomplete migration scope.
- **How to fix:** Add a "ViewModels" section to the Impact Summary. Note that `m_highlightedIds` (`QVariantMap`) remains string-keyed because QVariantMap keys are always QString -- conversion happens at the boundary when populating from `HighlightInfo`.

---

**4. ~~FIXED~~ `Person::createWithId` and `Family::createWithId` factory methods not mentioned.**

- **Location in design:** Not mentioned in the Models table (lines 315--325).
- **Source files:** `src/models/Person.h` lines 35--46 (`Person::createWithId(const QString& id, ...)`), `src/models/Family.h` lines 27--32 (`Family::createWithId(const QString& id, ...)`)
- **What's wrong:** These factory methods accept raw `QString` IDs and are used by the import services for ID preservation during PDF import. With strong IDs, they should accept `const PersonId&` and `const FamilyId&` respectively.
- **Why it matters:** These are entry points where external strings become entity IDs. Missing them creates a type-safety gap at the import boundary.
- **How to fix:** Add to the Models table: `Person::createWithId(const QString&, ...)` becomes `Person::createWithId(const PersonId&, ...)`, and likewise for `Family::createWithId`.

---

**5. ~~FIXED~~ Several Document methods missing from the Document Method Signatures table.**

- **Location in design:** Lines 327--350 (Document Method Signatures table).
- **Source file:** `src/models/Document.h`
- **Missing methods:**
  - `removeTeam(const QString& id)` (line 82) -- becomes `removeTeam(const TeamId&)`
  - `removeTag(const QString& id)` (line 92) -- becomes `removeTag(const TagId&)`
  - `setFamilies(const QHash<QString, Family>&)` (line 72) -- becomes `setFamilies(const QHash<FamilyId, Family>&)`
  - `setEqDistricts(const QHash<QString, MinisteringDistrict>&)` (line 120) -- becomes `setEqDistricts(const QHash<MinisteringDistrictId, MinisteringDistrict>&)`
  - `setEqGroups(const QHash<QString, MinisteringGroup>&)` (line 121) -- same pattern
  - `setRsDistricts(...)` (line 131) -- same pattern
  - `setRsGroups(...)` (line 132) -- same pattern
  - `familiesInStake(const QString&)` (line 74) and `familiesInWard(const QString&)` (line 75) -- these take ward/stake unit numbers and should remain `QString`, but should be explicitly listed as exclusions
- **Why it matters:** Incomplete table makes it easy to miss methods during implementation.
- **How to fix:** Add these methods to the table. For the `familiesInStake`/`familiesInWard` methods, add a note that they remain `QString` (ward/stake unit numbers).

---

**6. ~~FIXED~~ `SetFamiliesCommand`, `ImportEQMinisteringCommand`, and `ImportRSMinisteringCommand` missing from Commands table.**

- **Location in design:** Lines 430--447 (Commands section).
- **Source files:**
  - `src/commands/FamilyCommands.h` lines 49--64 -- `SetFamiliesCommand` holds `QHash<QString, Family>` for `m_newFamilies` and `m_oldFamilies`
  - `src/commands/ImportEQMinisteringCommand.h` lines 17--42 -- holds `QHash<QString, MinisteringDistrict>`, `QHash<QString, MinisteringGroup>`, `QHash<QString, Family>` for both new and previous state
  - `src/commands/ImportRSMinisteringCommand.h` lines 17--42 -- same pattern
- **What's wrong:** The design's Commands table only lists `ImportWardDirectoryCommand`. The catch-all line 447 ("Family/Team/Tag/... Add/Update/Delete commands hold the model struct directly") does not cover these commands, which hold `QHash<QString, Model>` maps keyed by entity ID.
- **Why it matters:** These hash map keys are entity IDs and must change to typed IDs. Missing them from the design table means they could be overlooked.
- **How to fix:** Add rows for `SetFamiliesCommand` (keys become `FamilyId`), `ImportEQMinisteringCommand` (keys become `MinisteringDistrictId`, `MinisteringGroupId`, `FamilyId`), and `ImportRSMinisteringCommand` (same).

---

**7. ~~FIXED~~ Existing `ScopeBuilder::batchModified()` migration path not stated.**

- **Location in design:** Lines 258--273 (Scope Builders section).
- **Source file:** `src/models/DocumentChange.cpp` line 20 (`ScopeBuilder::batchModified()`)
- **What's wrong:** The current `ScopeBuilder` has a `batchModified()` method. The design replaces this with `full()` on each typed scope builder, and states that `BatchModified` is absorbed into `Full` (line 210). However, the design never explicitly says that existing `DocumentChange::family().batchModified()` call sites become `DocumentChange::family().full()`.
- **Why it matters:** An implementer could be confused about what replaces `batchModified()` at call sites, since the old `batchModified()` set `ChangeAction::BatchModified` with no entity ID, while the new `full()` sets `ChangeAction::Full` with no entity ID. They are semantically the same, but the rename should be explicitly called out.
- **How to fix:** Add a sentence to the Scope Builders section: "Existing `scope.batchModified()` calls become `scope.full()` -- scoped `full()` means the entire collection of that type changed."

---

### Minor (Nice to Have)

**8. ~~FIXED~~ `FamilyMarkerProvider` interface implementors not listed in design.**

- **Location in design:** Lines 534--538 cover the `FamilyMarkerProvider` interface changes.
- **Source files:** `src/widgets/WardListView.h` line 28, `src/widgets/WardListDialog.h` line 54, `src/widgets/NeedsSubView.h` line 25, `src/widgets/EmergencyAssetView.h` line 29, `src/widgets/MinisteringView.h` line 22, `src/widgets/MinisteringTabView.h` line 28, `src/widgets/MinisteringTreeView.h` line 21, `src/widgets/UnassignedTreeView.h` line 23
- **What's wrong:** The design correctly specifies that `FamilyMarkerProvider::visibleFamilyIds()` returns `QSet<FamilyId>`, but does not list the 8 classes that implement (override) this method. Since the interface changes, all overrides must change too.
- **Why it matters:** This is a transitive change that the compiler will enforce, so it is not a correctness risk. However, listing the affected overrides would give implementers better scope estimation.
- **How to fix:** Add a note under the FamilyMarkerProvider entry: "All implementors (WardListView, WardListDialog, NeedsSubView, EmergencyAssetView, MinisteringView, MinisteringTabView, MinisteringTreeView, UnassignedTreeView) update their overrides accordingly."

---

**9. `DocumentChange` struct has no enforcement that at most one optional field is set.**

- **Location in design:** Line 229 ("At most one is set")
- **What's wrong:** The struct has 8 optional ID fields plus `metadataValue`. The "at most one is set" invariant is documented but not enforced -- someone could construct a `DocumentChange` with both `familyId` and `teamId` set via direct member assignment.
- **Why it matters:** In practice this is unlikely since all construction goes through scope builders, which only set one field each. But the invariant is invisible at the type level.
- **How to fix:** Consider adding a note that the struct relies on builder construction, and direct member assignment should be avoided. Optionally, a `Q_ASSERT` in consumers could verify the invariant in debug builds:
```cpp
Q_ASSERT((familyId.has_value() ? 1 : 0) + (teamId.has_value() ? 1 : 0) + ... <= 1);
```

---

**10. ~~FIXED~~ `MapViewModel::m_highlightedIds` remains `QVariantMap` (string-keyed) -- should be noted as a boundary point.**

- **Location in design:** Not mentioned.
- **Source file:** `src/viewmodels/MapViewModel.h` lines 24, 40, 92
- **What's wrong:** `m_highlightedIds` is a `QVariantMap` (i.e., `QMap<QString, QVariant>`) where keys are family ID strings. Since `QVariantMap` keys must be `QString`, this remains string-keyed with conversion at the boundary.
- **Why it matters:** This is an acceptable boundary case, but it should be documented so implementers know that ID-to-string conversion is intentional here, not an oversight.
- **How to fix:** Add a note to a "Boundary Points" subsection or to "What Does Not Change": "MapViewModel's QVariant-based properties remain string-keyed for QML/QVariantMap compatibility."

---

**11. ~~FIXED~~ `Filter` public getters/setters not listed in the Filter impact table.**

- **Location in design:** Lines 352--364 (Filter section).
- **Source file:** `src/listmodels/Filter.h` lines 43--45, 58--60
- **What's wrong:** The design lists the member fields and private helper methods, but the public getter/setter signatures also change:
  - `tagIds()` returns `QSet<QString>` -> `QSet<TagId>`
  - `teamIds()` returns `QSet<QString>` -> `QSet<TeamId>`
  - `assetTypeIds()` returns `QSet<QString>` -> `QSet<EmergencyAssetId>`
  - `setTagIds(const QSet<QString>&)` -> `setTagIds(const QSet<TagId>&)`
  - `setTeamIds(const QSet<QString>&)` -> `setTeamIds(const QSet<TeamId>&)`
  - `setAssetTypeIds(const QSet<QString>&)` -> `setAssetTypeIds(const QSet<EmergencyAssetId>&)`
- **Why it matters:** These are the public API that `FilterBar.cpp` calls extensively (lines 116--127, 130--140, 216--227, 317--328, 353--364, 577--590). Listing them makes the scope clearer.
- **How to fix:** Add the getter/setter signatures to the Filter table, or note that they change to match the member types.

---

### Style Violations

**12. Consumer pattern example uses one-liner case statements.**

- **Location in design:** Lines 292--294
- **What's wrong:** The example code shows:
```cpp
case ChangeAction::Updated:  updateFamilyRow(*change.familyId);  break;
case ChangeAction::Added:    insertFamilyRow(*change.familyId);  break;
case ChangeAction::Removed:  removeFamilyRow(*change.familyId);  break;
```
Per CODING_STYLE.md: "Never use one-liner case statements. Always put the body on a new line."
- **How to fix:** Reformat to:
```cpp
case ChangeAction::Updated:
    updateFamilyRow(*change.familyId);
    break;
case ChangeAction::Added:
    insertFamilyRow(*change.familyId);
    break;
case ChangeAction::Removed:
    removeFamilyRow(*change.familyId);
    break;
```

---

## Recommendations

1. **Add Services and ViewModels sections** to the Impact Summary. The services layer (`BackgroundGeocodingService`, `MinisteringImportService`, `WardDirectoryImportService`, `PersonMatching`, `DocumentManager`) and the viewmodel layer (`MapViewModel`) both contain entity ID usages that need conversion.

2. **Add `Person::createWithId` and `Family::createWithId`** to the Models impact table. These are important boundary points where raw strings enter the system during import.

3. **Replace C++20 `std::derived_from` constraint** with a C++17-compatible SFINAE alternative in the `SelectionKey::from` template.

4. **Complete the Document method signatures table** -- `removeTeam`, `removeTag`, `setFamilies`, `setEqDistricts`, `setEqGroups`, `setRsDistricts`, `setRsGroups` are all missing.

5. **Complete the Commands table** -- `SetFamiliesCommand`, `ImportEQMinisteringCommand`, and `ImportRSMinisteringCommand` hold `QHash<QString, Model>` maps that need typed keys.

6. **Explicitly state the `batchModified() -> full()` migration** in the Scope Builders section to avoid confusion during implementation.

7. **Consider adding a "Boundary Points" note** listing where `QString <-> TypedId` conversion happens: JSON serialization, QVariant-based QML interfaces (MapViewModel), and signal parameters at service boundaries.

---

## Assessment

**Overall quality:** Good

**Reasoning:** The core type system design (CRTP base, derived classes, `std::optional` semantics, SelectionKey, Tag split, DocumentChange redesign) is architecturally sound and well-justified. The design correctly handles the hardest cases: polymorphic tree node IDs, composite selection keys, the Tag entity-ids split, and the DocumentChange scope redesign. The C++20 issue is a compile-blocker but has a straightforward fix. The remaining issues are all about completeness of the impact tables -- roughly 8--10 files with entity ID usage are not mentioned. These are mostly straightforward transitive changes (service layer, view models, import commands), but an implementer relying solely on this document would discover them during compilation rather than during planning. Adding those files to the impact tables would make this design document excellent.
