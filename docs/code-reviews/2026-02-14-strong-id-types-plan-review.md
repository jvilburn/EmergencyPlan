# Plan Review: Strong ID Types Implementation Plan

**Date:** 2026-02-14
**Reviewed:** `docs/plans/2026-02-14-strong-id-types-plan.md`
**Against:** `docs/plans/2026-02-11-strong-id-types-design.md`
**Scope:** Discrepancies, gaps, and errors — not improvements

---

## Issues

### 1. ~~Tag toJson/fromJson format mismatch (Task 4)~~ — accepted

Format change is intentional. No backward compatibility needed.

### 2. ~~MinisteringCommands may need .cpp changes (Task 7)~~ — fixed

Verified: MinisteringCommands.cpp has no `batchModified()` calls (only individual CRUD `documentChange()` returns). Plan updated to note this.

### 3. ~~"Design question" left unresolved (Task 8)~~ — fixed

Decision made: `FamilyMemberMatchResult::familyId` and `FamilyReplacementResult::replacedFamilyId` both become `std::optional<FamilyId>`. Plan updated.

### 4. ~~WardListView::visibleFamiliesChanged signal type unspecified (Task 12)~~ — fixed

Plan updated to `visibleFamiliesChanged(const QList<FamilyId>&)`.

### 5. ~~PlaceholderView not explicitly called out (Task 12)~~ — fixed

Plan updated to call out `PlaceholderView` (internal class in MainWindow.cpp) and its `visibleFamilyIds()` override.

### 6. ~~MinisteringModel TreeNode — secondaryId mapping not explicit (Task 10)~~ — fixed

Plan updated with per-ItemType field mapping tables for both MinisteringModel and UnassignedMinisteringModel, showing which typed field replaces `id` and `secondaryId` for each node type.

---

## Non-Issues Verified

- Line number references are accurate against the current source (spot-checked ~14 files)
- The scoped `full()` delegating to global `full()` is equivalent to the design's consumer pattern (both treat `action == Full` as rebuild-everything regardless of scope fields)
- MinisteringDistrict's `m_presidencyMemberId` is correctly described as `std::optional<QString>` -> `std::optional<PersonId>` (matches current source)
- Q_DECLARE_METATYPE registrations are correctly included despite not being in the design

---

## Assessment

The plan is thorough and well-structured. All issues resolved.
