# Emergency Plan Qt - Design Guide

## Platform Strategy

This application uses **platform-native styling**:

| Platform | Approach |
|----------|----------|
| **Windows** | Windows 7 / Skeuomorphic - custom QSS styling |
| **macOS** | Native Aqua - no custom styling, follows Apple HIG |

---

## macOS Design

On macOS, the application uses **native styling** with no custom modifications:

- Native Aqua controls (buttons, scrollbars, inputs)
- System font (San Francisco)
- Native window chrome and title bar
- Standard macOS dialogs and sheets
- Respects system dark/light mode preference

**No custom QSS is loaded on macOS.** The `skeuomorphic.qss` stylesheet is Windows-only.

### macOS-Specific Considerations

```cpp
#ifdef Q_OS_MACOS
    // Native macOS style
    app.setStyle(QStyleFactory::create("macos"));

    // Respect system appearance (light/dark mode)
    // Qt 6 handles this automatically

    // Use native file dialogs
    QFileDialog::DontUseNativeDialog  // Don't set this flag
#endif
```

### Menu Bar
- On macOS, the menu bar moves to the system menu bar (top of screen)
- Use `menuBar()->setNativeMenuBar(true)` (default on macOS)
- Add standard macOS menu items (About, Preferences in app menu)

### Keyboard Shortcuts
- Use `QKeySequence::StandardKey` for cross-platform shortcuts
- Qt automatically maps Ctrl to Cmd on macOS

---

## Windows Design Philosophy

The Windows version uses a **Windows 7 / Skeuomorphic** design language. The goal is to create an interface that feels tactile, familiar, and visually rich with depth and dimension.

---

## Windows Core Design Principles

### 1. Depth and Dimension
- Buttons should look pressable with beveled edges and gradients
- Panels should have subtle shadows and raised/recessed appearances
- Use highlights and shadows to create 3D effects
- Toolbars and sidebars should feel like physical surfaces

### 2. Visual Richness
- Icons should have detail, shading, and perspective
- Use gradients rather than flat solid colors
- Incorporate subtle textures where appropriate
- Borders should have dimension (not just 1px flat lines)

### 3. Familiar Affordances
- Buttons look like buttons you can click
- Text fields look like places you can type
- Scrollbars have visible grip areas
- Tabs look like physical folder tabs

### 4. Warm Color Palette
- Avoid stark whites and pure blacks
- Use warm grays and off-whites for backgrounds
- Blues with depth for primary actions
- Subtle color gradients for visual interest

---

## Windows Qt Style Approach

### Primary: Windows Vista Style
Qt's Windows Vista style provides a good foundation with:
- Glass-like effects
- Gradient buttons
- Proper depth on controls

```cpp
// In main.cpp - Windows styling (macOS uses native, see above)
#ifdef Q_OS_WIN
    app.setStyle(QStyleFactory::create("windowsvista"));

    // Load custom skeuomorphic stylesheet
    QFile styleFile(":/styles/skeuomorphic.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        app.setStyleSheet(styleFile.readAll());
    }
#endif
```

### Secondary: Custom QSS Stylesheets
On Windows, Qt Style Sheets enhance the Vista base:

```css
/* Example: Skeuomorphic button styling */
QPushButton {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #f0f0f0,
        stop: 0.4 #e8e8e8,
        stop: 0.5 #e0e0e0,
        stop: 1.0 #d8d8d8
    );
    border: 1px solid #a0a0a0;
    border-radius: 3px;
    padding: 5px 15px;
    min-height: 22px;
}

QPushButton:hover {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #f8f8ff,
        stop: 0.4 #e8f0ff,
        stop: 0.5 #d8e8ff,
        stop: 1.0 #c8d8f0
    );
    border: 1px solid #6090c0;
}

QPushButton:pressed {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #c8c8c8,
        stop: 0.4 #d0d0d0,
        stop: 0.5 #d8d8d8,
        stop: 1.0 #e0e0e0
    );
    border: 1px solid #808080;
    padding-top: 6px;
    padding-bottom: 4px;
}

/* Toolbar with subtle gradient */
QToolBar {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #f8f8f8,
        stop: 1.0 #e0e0e0
    );
    border-bottom: 1px solid #b0b0b0;
    spacing: 3px;
    padding: 3px;
}

/* Group boxes with dimension */
QGroupBox {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #ffffff,
        stop: 1.0 #f0f0f0
    );
    border: 1px solid #c0c0c0;
    border-radius: 5px;
    margin-top: 10px;
    padding-top: 10px;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 5px;
    background: #f0f0f0;
}

/* Text inputs with inset appearance */
QLineEdit, QTextEdit, QSpinBox, QDoubleSpinBox {
    background: white;
    border: 1px solid #a0a0a0;
    border-radius: 2px;
    padding: 3px;
    selection-background-color: #3399ff;
}

QLineEdit:focus, QTextEdit:focus {
    border: 1px solid #3399ff;
}

/* List views with classic appearance */
QListView, QTreeView, QTableView {
    background: white;
    border: 1px solid #a0a0a0;
    alternate-background-color: #f5f5f5;
}

QListView::item:selected, QTreeView::item:selected {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #3399ff,
        stop: 1.0 #2277dd
    );
    color: white;
}

/* Tabs with folder-like appearance */
QTabWidget::pane {
    border: 1px solid #c0c0c0;
    background: #f8f8f8;
}

QTabBar::tab {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #f0f0f0,
        stop: 1.0 #d8d8d8
    );
    border: 1px solid #c0c0c0;
    border-bottom: none;
    padding: 5px 10px;
    margin-right: 2px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
}

QTabBar::tab:selected {
    background: #f8f8f8;
    border-bottom: 1px solid #f8f8f8;
}

QTabBar::tab:!selected {
    margin-top: 2px;
}

/* Scrollbars with visible grip */
QScrollBar:vertical {
    background: #f0f0f0;
    width: 17px;
    margin: 17px 0 17px 0;
    border: 1px solid #c0c0c0;
}

QScrollBar::handle:vertical {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #e8e8e8,
        stop: 0.5 #d0d0d0,
        stop: 1.0 #e8e8e8
    );
    min-height: 30px;
    border: 1px solid #a0a0a0;
    border-radius: 2px;
    margin: 1px;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #f0f0f0,
        stop: 1.0 #d8d8d8
    );
    height: 16px;
    border: 1px solid #c0c0c0;
}

/* Menu bar with classic Windows look */
QMenuBar {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #f8f8f8,
        stop: 1.0 #e8e8e8
    );
    border-bottom: 1px solid #c0c0c0;
}

QMenuBar::item:selected {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #d8e8ff,
        stop: 1.0 #b8d0f0
    );
    border: 1px solid #90b0d0;
    border-radius: 2px;
}

/* Dropdown menus with shadow effect */
QMenu {
    background: #f8f8f8;
    border: 1px solid #a0a0a0;
    padding: 2px;
}

QMenu::item {
    padding: 4px 25px 4px 20px;
}

QMenu::item:selected {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #3399ff,
        stop: 1.0 #2277dd
    );
    color: white;
}

/* Status bar */
QStatusBar {
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 #e8e8e8,
        stop: 1.0 #d0d0d0
    );
    border-top: 1px solid #a0a0a0;
}
```

---

## Color Palette

### Primary Colors
| Name | Hex | Usage |
|------|-----|-------|
| Window Background | #f0f0f0 | Main window, dialogs |
| Panel Background | #f8f8f8 | Panels, group boxes |
| Content Background | #ffffff | List views, text fields |
| Border Light | #c0c0c0 | Standard borders |
| Border Dark | #a0a0a0 | Emphasized borders |

### Accent Colors
| Name | Hex | Usage |
|------|-----|-------|
| Selection Blue | #3399ff | Selected items |
| Hover Blue | #d8e8ff | Hover states |
| Focus Blue | #2277dd | Focus indicators |
| Link Blue | #0066cc | Hyperlinks |

### Status Colors
| Name | Hex | Usage |
|------|-----|-------|
| Success Green | #339933 | Good status, recent visits |
| Warning Amber | #cc9900 | Attention needed |
| Alert Red | #cc3333 | Overdue, errors |
| Info Blue | #3366cc | Information |

### Team/Category Colors (Windows 7 palette-inspired)
| Name | Hex | Usage |
|------|-----|-------|
| Medical Blue | #4488cc | Medical skills |
| Recovery Green | #44aa44 | Recovery skills |
| Communication Orange | #cc8844 | Communication resources |
| Special Needs Red | #cc4444 | Special needs |

---

## Icons

### Style Guidelines
- **16x16** for toolbar buttons and small indicators
- **24x24** for main toolbar and list items
- **32x32** for dialog icons and large buttons
- **48x48** for application icon and splash

### Design Characteristics
- Use gradients and shading for depth
- Include subtle shadows and highlights
- Perspective angles for 3D appearance
- Detailed but recognizable at small sizes
- Consistent lighting (top-left light source)

### Icon Categories Needed
1. **File Operations**: New, Open, Save, Save As, Export, Import
2. **Edit Operations**: Undo, Redo, Cut, Copy, Paste, Delete
3. **Navigation**: Home, Back, Forward, Refresh
4. **View Modes**: EQ, RS, Emergency, Tags
5. **Entities**: Family, Person, Team, Tag, Resource
6. **Actions**: Add, Edit, Remove, Search, Filter
7. **Status**: Success, Warning, Error, Info
8. **Map**: Markers (home, medical, recovery, special, antenna)

### Recommended Icon Sources
- **Fugue Icons** (3,570 icons, Windows 7 era style)
- **Silk Icons** (1,000 icons, classic web 2.0 style)
- **Crystal Project** (Open source, skeuomorphic)
- **Oxygen Icons** (KDE, detailed and colorful)
- Custom created to match style

---

## Widget Specifications

### Buttons

**Standard Button**
```
┌─────────────────────────────┐
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓   │  ← Light gradient top
│  ░░░░░░░░░░░░░░░░░░░░░░   │
│      Button Text           │  ← Centered text
│  ░░░░░░░░░░░░░░░░░░░░░░   │
│  ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒   │  ← Darker gradient bottom
└─────────────────────────────┘
│ 1px border with slight 3D effect
```

**Primary Action Button**
- Use blue gradient instead of gray
- Slightly more prominent border
- Same pressed state behavior (shift down)

### List Items

**Family List Tile**
```
┌─────────────────────────────────────────────────────┐
│ ┌─────┐                                             │
│ │Icon │  Smith Family                    [Badges]   │
│ │     │  123 Main Street                            │
│ └─────┘  3 members                                  │
│ ▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔ │
└─────────────────────────────────────────────────────┘
  │ Subtle separator line between items
```

- Alternating row colors (#ffffff / #f5f5f5)
- Selection highlight with blue gradient
- Hover state with light blue background
- Team color indicator on left edge

### Dialogs

**Dialog Layout**
```
┌─────────────────────────────────────────┐
│ ▓▓▓▓▓▓▓▓▓▓ Dialog Title ▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │  ← Title bar gradient
├─────────────────────────────────────────┤
│                                         │
│   Form fields and content               │
│   with proper spacing                   │
│                                         │
├─────────────────────────────────────────┤
│                      [ Cancel ] [ OK ]  │  ← Button row
└─────────────────────────────────────────┘
```

- 10-15px padding around content
- 8px spacing between form rows
- Button row right-aligned with 8px spacing
- Group boxes for logical sections

### Sidebar

**Navigation Sidebar**
```
┌───────────────────────┐
│ ▓▓▓ Ward List ▓▓▓▓▓▓ │  ← Selected tab (raised)
├───────────────────────┤
│ ░░░ Ministering ░░░░ │  ← Unselected tab
├───────────────────────┤
│ ░░░ Emergency ░░░░░░ │
├───────────────────────┤
│ ░░░ Tags ░░░░░░░░░░░ │
├───────────────────────┤
│                       │
│   Content area        │
│   for selected tab    │
│                       │
│                       │
└───────────────────────┘
```

- Tabs with gradient backgrounds
- Selected tab appears raised/connected to content
- Content area with subtle inset border

---

## Map Markers

### Marker Design
- **Size**: 32x40 pixels (pin shape)
- **Style**: 3D appearance with shadow
- **Base color**: Warm orange for home marker
- **Overlays**: Colored indicators for special attributes

### Marker Components
```
       ╱╲
      ╱  ╲       ← Top highlight
     │    │
     │ ⌂  │      ← Icon area with gradient
     │    │
      ╲  ╱       ← Bottom shadow
       ╲╱
        │        ← Pin point with drop shadow
```

### Decoration Indicators
- **Medical**: Blue pill/cross overlay (top-right)
- **Recovery**: Green hammer overlay (top-left)
- **Communication**: Orange antenna overlay (top)
- **Special Needs**: Red alert overlay (bottom)

---

## Animations and Transitions

Keep animations subtle and purposeful:
- **Hover effects**: 100-150ms fade
- **Button press**: Immediate visual feedback
- **Panel expansion**: 200ms slide
- **Selection changes**: 100ms highlight transition

Avoid:
- Bouncing or elastic effects
- Long fade-ins/fade-outs
- Spinning or rotating elements
- Parallax effects

---

## Typography

### Font Family
- **Primary**: Segoe UI (Windows default)
- **Fallback**: Tahoma, Arial, sans-serif
- **Monospace**: Consolas, Courier New

### Font Sizes
| Element | Size | Weight |
|---------|------|--------|
| Window Title | 11pt | Bold |
| Section Header | 10pt | Bold |
| Body Text | 9pt | Normal |
| Small Labels | 8pt | Normal |
| List Items | 9pt | Normal |

### Text Colors
- **Primary text**: #000000 or #202020
- **Secondary text**: #606060
- **Disabled text**: #a0a0a0
- **Link text**: #0066cc
- **Error text**: #cc0000

---

## Spacing Guidelines

### Standard Spacing Units
- **4px**: Tight spacing (within related items)
- **8px**: Standard spacing (between elements)
- **12px**: Medium spacing (between sections)
- **16px**: Large spacing (between major sections)
- **24px**: Extra large (dialog margins)

### Form Layouts
- Label-to-field: 4px vertical
- Field-to-field: 8px vertical
- Group-to-group: 16px vertical
- Side margins: 12-16px

---

## Accessibility Considerations

Even with skeuomorphic design, maintain accessibility:

1. **Contrast**: Ensure 4.5:1 contrast ratio for text
2. **Focus indicators**: Clear visual focus rings
3. **Keyboard navigation**: All controls accessible via keyboard
4. **Screen readers**: Proper labels and descriptions
5. **Scalability**: UI should handle 125% and 150% DPI scaling

---

## Implementation Notes

### Loading the Stylesheet
```cpp
// In main.cpp or MainWindow constructor
QFile styleFile(":/styles/skeuomorphic.qss");
if (styleFile.open(QFile::ReadOnly)) {
    QString styleSheet = styleFile.readAll();
    app.setStyleSheet(styleSheet);
}
```

### Custom Widget Painting
For complex widgets that can't be styled with QSS:
```cpp
void CustomWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw gradient background
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0, QColor(248, 248, 248));
    gradient.setColorAt(1, QColor(232, 232, 232));
    painter.fillRect(rect(), gradient);

    // Draw border with slight 3D effect
    painter.setPen(QColor(192, 192, 192));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    // Draw highlight on top edge
    painter.setPen(QColor(255, 255, 255, 128));
    painter.drawLine(1, 1, width() - 2, 1);
}
```

### Shadow Effects
```cpp
// Add drop shadow to a widget
QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect;
shadow->setBlurRadius(10);
shadow->setOffset(2, 2);
shadow->setColor(QColor(0, 0, 0, 60));
widget->setGraphicsEffect(shadow);
```

---

## Reference Images

For visual reference, the design should evoke:
- Windows 7 Aero theme (without transparency)
- Microsoft Office 2007/2010 ribbon and controls
- Classic web applications from 2008-2012 era
- Physical desktop accessories (calculators, notepads)

The goal is an interface that feels solid, trustworthy, and professionally crafted rather than trendy or minimal.
