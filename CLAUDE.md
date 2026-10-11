@~/.claude/CLAUDE.md

# Claude Session Notes

## Project Overview
Qt 6 / C++17 port of a Flutter ward planning application for LDS church use. Features family management, ministering tracking, emergency response planning, and map visualization.

## Coding Style
See [CODING_STYLE.md](docs/CODING_STYLE.md) for complete details:
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
- **Qt Version**: 6.11.0 (msvc2022_64)
- **Build**: Use **Ctrl+Shift+B** in VS Code, or run `build.bat` directly
- **Running the app**: the user runs it manually

**Building and testing from Claude's shell:**
```bash
cmd //c "build.bat debug" > build.log 2>&1
PATH="/c/Qt/6.11.0/msvc2022_64/bin:$PATH" QT_FORCE_STDERR_LOGGING=1 \
    build/windows-debug/bin/WardPlanningTests.exe > test.log 2>&1
```
Write both logs to the scratchpad. Summarize the test log with `grep -n "^FAIL\|QFATAL"` and the summed `Totals` lines. The test binary runs only with Qt's `bin` on `PATH`, and prints only with `QT_FORCE_STDERR_LOGGING=1`, because Claude's shell has no console and Qt otherwise logs to the debugger. The conformance run against a real server is skipped unless `WARD_PLANNING_SERVER_URL` is set.

**IMPORTANT**: Do NOT run `cmake` directly from Git Bash or MSYS terminals. The MSVC environment is not set up in those shells, causing errors like "Cannot open include file: 'type_traits'". `build.bat` calls `vcvars64.bat` itself, which is why it works through `cmd`.

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

### libsodium (document encryption and signing)
- **Source**: vcpkg (`libsodium`, 1.0.22 or later, pinned in `vcpkg.json`)
- **Target**: `unofficial-sodium::sodium`; the static port writes `SODIUM_STATIC` into its own `export.h`
- **Wrapper**: `DocumentCrypto.h/cpp` - every other file reaches libsodium through it, apart from `CryptoTypes.cpp` (size checks) and the tests
- `DocumentCrypto::initialise()` runs in `main()` and in the test runner before any other call

### Device identity
- `IdentityFile.h/cpp` reads and writes the identity file: this device's secret halves, owner-only, as SSH keys are kept; public halves are derived on load
- Device state lives in `AppLocalDataLocation`, which does not roam: settings, the single-instance lock, and the identity file

## Key Design Decisions

### Document Collections
Using `QHash<QString, Model>` instead of `QList<Model>` because:
- Enforces uniqueness by ID automatically
- O(1) lookup by ID
- Display order is computed at render time (sorted by name, etc.)

## Architecture References
- [ARCHITECTURE.md](docs/ARCHITECTURE.md) - Application architecture
- [IMPORT_DESIGN.md](docs/IMPORT_DESIGN.md) - Import semantics for ward directory and ministering PDFs

---

## Active Tasks

| Task | Status | Plan Doc |
|------|--------|----------|
| NeedsSubView dialog UX | blocked:discussion | - |

**Status:** `ready`, `in-progress`, `blocked:<reason>`, `done` (then remove row + archive plan)

---


## Pending Discussion: NeedsSubView Dialog UX

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


## CRITICAL REMINDER

**DO NOT MAKE DECISIONS FOR THE USER.**

- If there's a choice to make, ASK.
- If something could be kept or removed, ASK.
- If a limitation exists, ASK if it should be accepted.
- If implementation is done, ASK if the component is complete.
- NEVER assume, infer, or decide on the user's behalf.
- NEVER move to the next task/component without explicit user confirmation.

**When in doubt: ASK.**
