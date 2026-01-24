@~/.claude/CLAUDE.md

# Claude Session Notes

## Project Overview
Qt 6 / C++17 port of a Flutter ward planning application for LDS church use. Features family management, ministering tracking, emergency response planning, and map visualization.

## Coding Style
See [CODING_STYLE.md](CODING_STYLE.md) for complete details:
- Allman-style braces (opening brace on new line)
- Only one-liner functions in headers; multi-line functions go in .cpp
- `std::optional<>` for nullable fields (no type aliases)
- Always use braces on control structures
- Leading operators on continuation lines
- Use `json` variable name in toJson/fromJson
- Value-based equality (compare all fields)
- **Use `tr()` for all user-visible strings** (for future internationalization)

## Build System
- **IDE**: VS Code (no CMake Tools extension needed)
- **Compiler**: MSVC 2022 (not MinGW - they're incompatible with Qt MSVC libs)
- **Generator**: Ninja
- **Qt Version**: 6.10.1 (msvc2022_64)
- **Build**: Use **Ctrl+Shift+B** in VS Code, or run `build.bat` directly
- **Testing**: User builds and runs the app manually - do NOT attempt to build/run from Claude

**IMPORTANT**: Do NOT run cmake builds from Git Bash or MSYS terminals. The MSVC environment is not set up in those shells, causing errors like "Cannot open include file: 'type_traits'". Use `build.bat` instead, which sets up the Visual Studio environment properly.

**IntelliSense**: Error squiggles are disabled in settings.json because they get stale after Claude Code edits. The compiler is the source of truth for errors.

Configuration files:
- `CMakePresets.json` - defines windows-debug/release presets with MSVC compiler
- `.vscode/tasks.json` - Build Debug task (runs build.bat)
- `vcpkg.json` - vcpkg manifest for dependencies
- `build.bat` - Incremental build script (only reconfigures when CMakeLists.txt changes)

## Third-Party Libraries

### MuPDF (PDF text extraction)
- **Source**: vcpkg (`libmupdf`)
- **Triplet**: `x64-windows-static-md` (static libs, dynamic CRT for Qt compatibility)
- **Wrapper**: `PdfExtractor.h/cpp` - C++ wrapper around MuPDF's C API
- **Dependencies**: Automatically pulls freetype, harfbuzz, openjpeg, libjpeg, libpng, zlib, jbig2dec, gumbo, brotli, bz2

To install/update MuPDF:
```
vcpkg install libmupdf:x64-windows-static-md
```

Note: MuPDF uses `fz_try/fz_catch/fz_always` macros (setjmp/longjmp) for error handling. Avoid C++ objects with destructors crossing these boundaries.

## Key Design Decisions

### Document Collections
Using `QHash<QString, Model>` instead of `QList<Model>` because:
- Enforces uniqueness by ID automatically
- O(1) lookup by ID
- Display order is computed at render time (sorted by name, etc.)

### Person Birth Date
Stored as separate `birthYear`, `birthMonth`, `birthDay` optionals because:
- Adults have "19 Nov" format (no year)
- Children have "19 Nov (15)" with age display
- `age()` helper used by `isChild()` and `ageDisplay()`

### MinisteringVisit
- Uses `QDate` not `QDateTime` (time not needed)
- Getter is `date()` not `contactDate()`

### Ministering Groups
- No `MinisteringGroupBase` class - avoided inheritance for cleaner value semantics
- EQ and RS groups are independent classes with duplicated common fields
- Uses `QDate` for interviewedDate (not QDateTime)

### Ministering Tab UI
- "Update from PDF" button at the top of the ministering list
- Allows importing/updating ministering assignments directly from the current view
- "Reset Proposed" button to copy current assignments to proposed (for starting fresh edits)

## Architecture References
- [CONVERSION_PLAN.md](CONVERSION_PLAN.md) - Full conversion plan from Flutter
- [ARCHITECTURE.md](ARCHITECTURE.md) - Application architecture
- [IMPLEMENTATION_CHECKLIST.md](IMPLEMENTATION_CHECKLIST.md) - Detailed task checklist (static progress tracker)

---

## Active Tasks

| Task | Status | Plan Doc |
|------|--------|----------|
| NeedsSubView dialog UX | blocked:discussion | - |

**Status:** `ready`, `in-progress`, `blocked:<reason>`, `done` (then remove row + archive plan)

---

## Current Work (2026-01-19)

### Completed
- **MapHighlightProvider Refactoring** - Done. Interface uses semantic `HighlightInfo` struct, all existing views (WardListView, MinisteringView, WardListDialog) migrated, legacy code removed.
- **Map Animation UI Padding** - Done. All tasks implemented and committed.
- **Emergency Tab Implementation** - Done. EmergencyView with Skills, Equipment, Needs sub-tabs. MapHighlightProvider integration working.
- **Ministering Expandable Groups** - Done. Companionships expand to show Ministers/Families/Sisters sections with lazy-loaded contact info (phone, email, address).

### Pending Discussion: NeedsSubView Dialog UX

The current `showAddNeedDialog()` and `showEditNeedDialog()` have a UX issue worth revisiting:

**Problem:** "Add Special Need" dialog allows selecting someone who already has a need, silently overwriting their existing note. The dialog title implies adding something new.

**Proposed solution:** Merge into a single "Special Need" dialog:
- Dialog title: "Special Need" (not "Add" or "Edit")
- When opened from empty space: user selects person/family, note pre-fills if they already have one
- When opened on existing item: person/family is locked (read-only), note pre-fills
- Both flows use `SetSpecialNeedCommand` (idempotent)
- Each special need is for ONE person OR ONE family - no "transferring"

This reframes the operation as "set special need" rather than "add/edit/delete", which better matches the underlying command semantics.

---

## Windows 7 Styling Task (2026-01-22)

### STRICT PROCESS - DO NOT DEVIATE

**Reference Documentation:**
- **7.css**: https://khang-nd.github.io/7.css/
- **Qt 6 Stylesheet Reference**: https://doc.qt.io/qt-6/stylesheet-reference.html

**NotebookLM (Preferred):** Both documents are in the `wardplanningapp` notebook. Use the `notebooklm` skill to query for comprehensive answers about component properties, pseudo-states, and supported features. Avoids context limits and provides source-grounded answers.

**Fallback (curl + local read):** If NotebookLM is unavailable:
```bash
curl -sL "https://unpkg.com/7.css" -o /tmp/7.css
curl -sL "https://doc.qt.io/qt-6/stylesheet-reference.html" -o /tmp/qt-stylesheet-ref.html
```

**For EVERY component, follow this process:**

1. Extract ALL properties from 7.css for that component (all states: normal, hover, active, disabled, focus)
2. Extract ALL properties from our windows7.qss for the Qt equivalent
3. Create a comparison table showing EVERY difference
4. **STOP AND WAIT**: Present a numbered list of ALL differences to the user - DO NOT implement anything yet
5. Wait for user to decide on EACH difference (match it, alternative, C++ implementation, or accept limitation)
6. Only after user has approved every item, implement the changes
7. After implementing, list any remaining unresolved differences
8. **ASK EXPLICITLY**: "Is this component done, or are there changes you'd like to make?"
9. Do NOT move to next component until user explicitly says "done" or equivalent

**NEVER:**
- Say a difference is "minor"
- Skip a property because it seems unimportant
- Assume Qt defaults are acceptable
- Use WebFetch to read reference documentation (it summarizes and misses details)
- Implement changes before user has approved ALL differences
- Decide to "omit" or "accept limitation" without asking user first
- Mark a component as DONE until user explicitly says it's done

**WebFetch Warning:** WebFetch uses AI summarization which WILL miss important details. For ANY reference material (Qt documentation, 7.css, API docs, etc.), ALWAYS download with curl and read locally:
```bash
curl -sL "https://example.com/docs.html" -o /tmp/docs.html
grep -i "search term" /tmp/docs.html
```

**Qt QSS Supported Properties** (from /tmp/qt-stylesheet-ref.html):
alternate-background-color, background, background-attachment, background-clip, background-color, background-image, background-origin, background-repeat, border, border-color, border-image, border-radius, border-style, border-width, bottom, button-layout, color, font, gridline-color, height, icon, icon-size, image, image-position, left, lineedit-password-character, lineedit-password-mask-delay, margin, max-height, max-width, messagebox-text-interaction-flags, min-height, min-width, opacity, padding, paint-alternating-row-colors-for-empty-area, placeholder-text-color, position, right, selection-background-color, selection-color, show-decoration-selected, spacing, subcontrol-origin, subcontrol-position, text-align, titlebar-show-tooltips-on-buttons, top, width

**NOT supported:** box-shadow (use border-image with pre-rendered shadow PNG instead)

### 7.css CSS Variables Reference

```
--w7-surface: #f0f0f0
--w7-el-bg: #f2f2f2
--w7-el-bg-d: #f4f4f4 (disabled)
--w7-el-bg-s-1: #ebebeb
--w7-el-bg-s-2: #cfcfcf
--w7-el-bd: #8e8f8f (border)
--w7-el-bd-h: #3c7fb1 (hover border)
--w7-el-bd-a: #6d91ab (active border)
--w7-el-bd-d: #adb2b5 (disabled border)
--w7-el-bdr: 3px (border radius)
--w7-el-c: #000 (text color)
--w7-el-c-d: #838383 (disabled text)
--w7-el-grad: linear-gradient(#f2f2f2 45%, #ebebeb 45%, #cfcfcf)
--w7-el-grad-h: linear-gradient(#eaf6fd 45%, #bee6fd 0, #a7d9f5)
--w7-el-grad-a: linear-gradient(#e5f4fc, #c4e5f6 30% 50%, #98d1ef 50%, #68b3db)
--w7-el-sd: inset 0 0 0 1px #fffc (inner shadow - needs border-image)
```

### Component Order (from 7.css)

1. [x] QToolTip - DONE (border-image with shadow PNG, no balloon pointer per user)
2. [x] QPushButton - DONE (inner shadow limitation accepted, default border fixed to #5586a3)
3. [x] QCheckBox - DONE (14px, PNG checkmarks, proper disabled state)
4. [x] QRadioButton - DONE (14px, PNG dots, proper disabled state)
5. [x] QComboBox - DONE (3px radius, PNG arrows, focus outline)
6. [x] QScrollBar - DONE (sharp 45% gradients, grip dots PNG, pressed states, transparent button borders)
7. [x] QProgressBar - DONE (15px height, 7.css track gradient, margin 2px 0, no chunk margin/radius)
8. [x] QSlider - DONE (3px track, PNG pointer thumbs, focus outline, disabled state)
9. [x] QTabBar/QTabWidget - DONE (sharp gradient, white selected, pane padding 14px)
10. [x] QMenu/QMenuBar - DONE (solid blue menubar hover, black border, subtle item hover, groove separator)
11. [x] QTreeView - DONE (border #8e8f8f, padding 6px 6px 6px 20px, item margin 4px, solid blue hover, down-right expand arrow, disabled opacity 0.5)
12. [x] QTableView/QHeaderView - DONE (7.css gradient, 22px header height, sort arrows PNG, corner button, item border-right #eee)
13. [x] QLineEdit/QTextEdit - DONE (hover colors matched, outline:none on focus, removed selection/disabled colors, QTextEdit padding matched)
14. [x] QGroupBox (fieldset) - DONE (border #cdd7db, 3px radius, margin 0, padding 8px 10px 10px, removed title styling)
15. [x] Final check for missing components - DONE (all 7.css components covered, Qt-only widgets kept as-is)

### Accepted Limitations

- **box-shadow (inset)** - Qt QSS doesn't support this. Inner white highlights on buttons/checkboxes/radios are omitted.
- **CSS animations** - Qt QSS doesn't support animations. Default button pulse effect is omitted.
- **CSS transitions** - Qt QSS doesn't support transitions. Hover effects are instant.
- **:last pseudo-state** - Qt QSS doesn't support `:last` for table items. Cell border-right applies to all cells (7.css omits it on last cell).

---

## CRITICAL REMINDER

**DO NOT MAKE DECISIONS FOR THE USER.**

- If there's a choice to make, ASK.
- If something could be kept or removed, ASK.
- If a limitation exists, ASK if it should be accepted.
- If implementation is done, ASK if the component is complete.
- NEVER assume, infer, or decide on the user's behalf.
- NEVER move to the next task/component without explicit user confirmation.

**When in doubt: ASK.**
