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

## High Priority Technical Debt

### Command::familyWithChangedAddress() - BAD DESIGN
The `familyWithChangedAddress()` virtual method on Command is a code smell. It's a special-case hook that only exists for geocoding, violating separation of concerns. When we implement `DocumentChange` (see docs/plans), this should be refactored so that:
- Commands return a `DocumentChange` describing what changed
- Geocoding logic observes family changes and checks if address field differs
- No command-specific hooks for individual side effects

## Architecture References
- [CONVERSION_PLAN.md](CONVERSION_PLAN.md) - Full conversion plan from Flutter
- [ARCHITECTURE.md](ARCHITECTURE.md) - Application architecture
- [IMPLEMENTATION_CHECKLIST.md](IMPLEMENTATION_CHECKLIST.md) - Detailed task checklist
