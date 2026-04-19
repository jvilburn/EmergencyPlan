# Emergency Plan Qt

A C++/Qt desktop application for managing ward/stake family data, emergency response teams, and resources.

## Platform Support

| Platform | UI Style | Status |
|----------|----------|--------|
| **Windows** | Windows 7 / Skeuomorphic | Primary target |
| **macOS** | Native Aqua | Secondary |

## Features

- **Family Management**: Track ward families with addresses, members, and contact information
- **Interactive Map**: View families on a map with custom markers and highlighting
- **Ministering Organization**: Manage EQ and RS ministering assignments and visits
- **Emergency Response**: Organize teams and track skills/equipment for emergency preparedness
- **Tag System**: Flexible categorization of families and individuals
- **PDF Import**: Import ward directories and ministering assignments from PDF

## Building

### Prerequisites

- CMake 3.21+
- Qt 6.11+ with modules: Core, Gui, Widgets, Network, Svg, Pdf, Concurrent, PrintSupport, Test
- C++17 compatible compiler
- Ninja build system
- vcpkg (for MuPDF dependency)

### Build Steps

**Windows (use build.bat, not Git Bash):**
```cmd
cd WardPlanningQt

# Debug build (default)
build.bat

# Release build
build.bat release

# Run
build\bin\EmergencyPlan.exe
```

The build script automatically:
- Sets up MSVC 2022 environment
- Configures with CMake presets (only when CMakeLists.txt changes)
- Builds with Ninja
- Deploys Qt DLLs with windeployqt

**macOS / Linux:**
```bash
cd WardPlanningQt

# Debug build (default)
./build.sh

# Release build
./build.sh release

# Run
./build/bin/EmergencyPlan        # Linux
open ./build/bin/EmergencyPlan.app  # macOS
```

## Development Tools

The project includes command-line tools for debugging and analysis (built when `BUILD_TOOLS=ON`, the default):

### pdf_dump

Dumps raw PDF text extraction data for analyzing font sizes, positions, and run classification.

```bash
# Basic usage - output to stdout
pdf_dump directory.pdf

# Save to file
pdf_dump -o analysis.txt directory.pdf
```

### test_import

Tests the full ward directory import pipeline and displays parsed family data.

```bash
# Parse all families
test_import directory.pdf

# Filter to specific family (case-insensitive)
test_import -f Smith directory.pdf
```

## Project Structure

```
WardPlanningQt/
├── src/
│   ├── models/          # Data model classes
│   ├── commands/        # Command pattern for undo/redo
│   ├── services/        # Business logic services
│   ├── viewmodels/      # UI state management
│   ├── listmodels/      # QAbstractItemModel wrappers
│   ├── widgets/         # Qt Widget UI components
│   ├── ui/              # Qt Designer .ui files
│   └── resources/       # Icons, styles, configs
├── tools/               # Development CLI tools (pdf_dump, test_import)
├── tests/               # Unit and integration tests
├── docs/                # Documentation
└── packaging/           # Installer configurations
```

## Documentation

- **docs/ARCHITECTURE.md** - Application architecture guide
- **docs/DESIGN_GUIDE.md** - Visual design specifications
- **docs/CODING_STYLE.md** - Coding conventions
- **docs/IMPORT_DESIGN.md** - Import semantics for ward directory and ministering PDFs
- **docs/BUILDING.md** - Build prerequisites and setup
- **docs/USER_GUIDE.md** - How to use the application

## License

GNU Affero General Public License v3.0 or later. See [LICENSE.txt](LICENSE.txt) for details.
