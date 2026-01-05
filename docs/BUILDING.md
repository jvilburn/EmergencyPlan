# Building Emergency Plan Qt

## Prerequisites

### Windows

1. **Visual Studio 2019/2022** (Community edition is free)
   - Install "Desktop development with C++" workload
   - Includes MSVC compiler and Windows SDK

2. **Qt 6.5+**
   - Download from https://www.qt.io/download-qt-installer
   - Select: Qt 6.x.x → MSVC 2019 64-bit
   - Required modules: Core, Gui, Widgets, Network, Svg, Pdf, Positioning, Concurrent

3. **CMake 3.21+**
   - Download from https://cmake.org/download/
   - Or install via Visual Studio

4. **Ninja** (recommended build system)
   - Install via: `winget install Ninja-build.Ninja`
   - Or download from https://ninja-build.org/

5. **VS Code Extensions**
   - Open the project folder, VS Code will prompt to install recommended extensions
   - Or manually install: C/C++, CMake Tools, CMake

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

4. **VS Code Extensions**
   - Same as Windows

---

## VS Code Setup

### 1. Open Project
```
File → Open Folder → Select EmergencyPlanQt directory
```

### 2. Install Extensions
When prompted, click "Install All" for recommended extensions, or install manually:
- **C/C++** (ms-vscode.cpptools)
- **CMake Tools** (ms-vscode.cmake-tools)
- **CMake** (twxs.cmake)

### 3. Configure Qt Path

**Option A: Edit CMakePresets.json**

Update the `CMAKE_PREFIX_PATH` in `CMakePresets.json` to match your Qt installation:

```json
"cacheVariables": {
    "CMAKE_PREFIX_PATH": "C:/Qt/6.6.0/msvc2019_64"  // Windows
    // "CMAKE_PREFIX_PATH": "/opt/homebrew/opt/qt@6"  // macOS Homebrew
    // "CMAKE_PREFIX_PATH": "$HOME/Qt/6.6.0/macos"    // macOS Qt installer
}
```

**Option B: Create CMakeUserPresets.json**

Copy the template and customize:
```bash
cp CMakeUserPresets.json.template CMakeUserPresets.json
```
Then edit `CMakeUserPresets.json` with your paths (this file is gitignored).

### 4. Select Kit and Configure

1. Press `Ctrl+Shift+P` (or `Cmd+Shift+P` on Mac)
2. Type "CMake: Select a Kit"
3. Choose your compiler:
   - Windows: "Visual Studio ... amd64"
   - macOS: "Clang ..."
4. CMake Tools will auto-configure the project

### 5. Build

- **Status Bar**: Click "Build" in the bottom status bar
- **Keyboard**: Press `F7` or `Ctrl+Shift+B`
- **Command Palette**: `CMake: Build`

### 6. Run / Debug

- **Run**: Click ▶ in status bar or press `Shift+F5`
- **Debug**: Press `F5` or click the Run and Debug sidebar

---

## Build Configurations

### Using CMake Presets (Recommended)

CMake Presets provide standardized build configurations:

```bash
# Configure
cmake --preset windows-debug    # or macos-debug

# Build
cmake --build --preset windows-debug

# Test
ctest --preset windows-debug
```

### Using CMake Tools Extension

The extension provides UI controls in the status bar:
- **Kit**: Compiler selection
- **Build Type**: Debug/Release
- **Build**: Build the project
- **Target**: Select build target
- **Launch**: Run/debug the application

---

## Project Structure After Build

```
EmergencyPlanQt/
├── build/
│   ├── bin/
│   │   ├── Debug/
│   │   │   └── EmergencyPlan.exe     (Windows)
│   │   └── EmergencyPlan.app/        (macOS)
│   ├── lib/
│   └── CMakeFiles/
├── src/
└── ...
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

Or use a different generator:
```json
"generator": "Visual Studio 17 2022"  // Windows
"generator": "Unix Makefiles"          // macOS/Linux
```

### IntelliSense not working

1. Wait for CMake configuration to complete
2. Check Output panel → CMake/C++ for errors
3. Reload VS Code: `Ctrl+Shift+P` → "Developer: Reload Window"

### Build errors with Qt

Ensure all required Qt modules are installed:
- Core, Gui, Widgets, Network, Svg, Pdf, Positioning, Concurrent

---

## Keyboard Shortcuts

| Action | Windows | macOS |
|--------|---------|-------|
| Build | `F7` or `Ctrl+Shift+B` | `F7` or `Cmd+Shift+B` |
| Debug | `F5` | `F5` |
| Run without Debug | `Ctrl+F5` | `Ctrl+F5` |
| Stop | `Shift+F5` | `Shift+F5` |
| Command Palette | `Ctrl+Shift+P` | `Cmd+Shift+P` |
| Go to File | `Ctrl+P` | `Cmd+P` |
| Go to Symbol | `Ctrl+Shift+O` | `Cmd+Shift+O` |
| Find in Files | `Ctrl+Shift+F` | `Cmd+Shift+F` |

---

## Additional Tools

### Qt Designer
For visual UI design (if using .ui files):
- Windows: `C:/Qt/6.x.x/msvc2019_64/bin/designer.exe`
- macOS: `open -a Designer`

### Qt Assistant
For Qt documentation:
- Windows: `C:/Qt/6.x.x/msvc2019_64/bin/assistant.exe`
- macOS: `open -a Assistant`

Or use VS Code task: `Terminal → Run Task → Open Qt Designer/Assistant`
