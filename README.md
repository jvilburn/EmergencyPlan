# Emergency Plan Qt

A C++/Qt desktop application for managing ward/stake family data, emergency response teams, and resources. This application is a conversion from the original Flutter/Dart implementation.

## Platform Support

| Platform | UI Style | Status |
|----------|----------|--------|
| **Windows** | Windows 7 / Skeuomorphic | Primary target |
| **macOS** | Native Aqua | Developed in parallel |

## Design Philosophy

### Windows
Classic **Windows 7 / Skeuomorphic** design featuring:
- Visual depth with gradients and shadows
- Tactile button and control appearances
- Warm color palette with dimensional effects
- Familiar classic Windows look

### macOS
**Native Aqua** styling:
- Standard macOS controls and appearance
- System menu bar integration
- Follows Apple Human Interface Guidelines
- Respects system dark/light mode

## Features

- **Family Management**: Track ward families with addresses, members, and contact information
- **Interactive Map**: View families on a map with custom markers and highlighting
- **Ministering Organization**: Manage EQ and RS ministering assignments and visits
- **Emergency Response**: Organize teams and track skills/equipment for emergency preparedness
- **Resource Management**: Track medical skills, recovery capabilities, communication resources
- **Tag System**: Flexible categorization of families and individuals
- **PDF Import/Export**: Import ward directories and export reports

## Building

### Prerequisites

- CMake 3.21+
- Qt 6.5+ with the following modules:
  - Core, Gui, Widgets
  - Network, Svg, Pdf
  - Positioning, Concurrent
  - Test (for running tests)
- C++17 compatible compiler

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

### Running Tests

```bash
cd build
ctest --output-on-failure
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

Output includes:
- Font sizes found on each page (largest to smallest)
- Text runs with Y position, X position, width, font size, bold flag, and text content

Useful for tuning PDF parsing heuristics when the parser encounters new PDF layouts.

### test_import

Tests the full ward directory import pipeline and displays parsed family data.

```bash
# Parse all families
test_import directory.pdf

# Filter to specific family (case-insensitive)
test_import -f Smith directory.pdf
```

Output includes:
- Ward name and unit number
- Family count
- For each family: family name, address, coordinates, primary/secondary contacts with phone/email/callings, and children

## Project Structure

```
WardPlanningQt/
├── src/
│   ├── models/          # Data model classes
│   ├── commands/        # Command pattern for undo/redo
│   ├── services/        # Business logic services
│   ├── viewmodels/      # UI state management
│   ├── listmodels/      # QAbstractItemModel wrappers
│   ├── controllers/     # UI logic coordinators
│   ├── widgets/         # Qt Widget UI components
│   └── resources/       # Icons, styles, configs
├── tools/               # Development CLI tools (pdf_dump, test_import)
├── tests/               # Unit and integration tests
├── docs/                # Documentation
└── packaging/           # Installer configurations
```

## Documentation

- **CONVERSION_PLAN.md** - Detailed conversion plan from Flutter
- **ARCHITECTURE.md** - Application architecture guide
- **DESIGN_GUIDE.md** - Visual design specifications
- **IMPLEMENTATION_CHECKLIST.md** - Implementation progress tracking

## License

[License information here]
