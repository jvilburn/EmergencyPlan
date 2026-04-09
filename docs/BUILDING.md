# Building Emergency Plan Qt

## Prerequisites

### Windows

1. **Visual Studio 2022** (Community edition is free)
   - Install "Desktop development with C++" workload
   - Includes MSVC compiler and Windows SDK

2. **Qt 6.11+**
   - Download from https://www.qt.io/download-qt-installer
   - Select: Qt 6.x.x → MSVC 2022 64-bit
   - Required modules: Core, Gui, Widgets, Network, Svg, Pdf, Concurrent, PrintSupport, Test

3. **CMake 3.21+**
   - Download from https://cmake.org/download/
   - Or install via Visual Studio

4. **Ninja** (build system)
   - Install via: `winget install Ninja-build.Ninja`
   - Or download from https://ninja-build.org/

5. **vcpkg** (for MuPDF dependency)
   - See vcpkg.json for the manifest

### macOS

1. **Xcode Command Line Tools**
   ```bash
   xcode-select --install
   ```

2. **Homebrew** (package manager)
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

3. **Qt 6, CMake, Ninja**
   ```bash
   brew install qt@6 cmake ninja
   ```

---

## Building

### Windows

**Important**: Do NOT build from Git Bash or MSYS terminals. The MSVC environment is not set up in those shells.

Use `build.bat` which sets up the Visual Studio environment automatically:

```cmd
# Debug build (default)
build.bat

# Release build
build.bat release
```

The build script:
- Sets up MSVC 2022 environment via vcvarsall.bat
- Runs CMake configure only when CMakeLists.txt changes
- Builds with Ninja
- Deploys Qt DLLs with windeployqt

### macOS / Linux

```bash
# Debug build (default)
./build.sh

# Release build
./build.sh release
```

---

## VS Code Setup

### IDE Configuration

The project includes pre-configured VS Code settings. The CMake Tools extension is **not needed** — building is handled by `build.bat`/`build.sh` via the VS Code build task.

**Build shortcut**: `Ctrl+Shift+B` (runs `build.bat`)

### Qt Path Configuration

Update `CMAKE_PREFIX_PATH` in `CMakePresets.json` to match your Qt installation:

```json
"cacheVariables": {
    "CMAKE_PREFIX_PATH": "C:/Qt/6.11.0/msvc2022_64"
}
```

Or create a `CMakeUserPresets.json` (gitignored) from the template:
```bash
cp CMakeUserPresets.json.template CMakeUserPresets.json
```

---

## Troubleshooting

### "Qt not found"

1. Verify Qt is installed correctly
2. Check `CMAKE_PREFIX_PATH` points to Qt installation
3. The path should contain `lib/cmake/Qt6/`

### "Ninja not found"

Install Ninja:
- Windows: `winget install Ninja-build.Ninja`
- macOS: `brew install ninja`

### Build errors with Qt

Ensure all required Qt modules are installed:
- Core, Gui, Widgets, Network, Svg, Pdf, Concurrent, PrintSupport, Test

### "Cannot open include file: 'type_traits'" (Windows)

You're building from Git Bash or MSYS. Use `build.bat` instead, which sets up the MSVC environment.
