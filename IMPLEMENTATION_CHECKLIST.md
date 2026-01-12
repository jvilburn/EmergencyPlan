# Emergency Plan Qt - Implementation Checklist

This checklist tracks the implementation progress for converting the Emergency Plan app from Flutter to C++ Qt.

---

## Phase 1: Project Foundation

### Build System Setup
- [x] Create root CMakeLists.txt
- [x] Configure C++17 standard
- [x] Add Qt6 find_package calls
- [x] Create cmake/ helper modules
- [x] Configure src/ subdirectory
- [x] Configure tests/ subdirectory
- [ ] Add resources.qrc file
- [x] Test basic build compiles

### Project Structure
- [x] Create src/models/ directory
- [x] Create src/commands/ directory
- [x] Create src/services/ directory
- [ ] Create src/viewmodels/ directory
- [x] Create src/widgets/ directory
- [ ] Create src/widgets/dialogs/ directory
- [ ] Create src/widgets/shared/ directory
- ~~Create src/qml/ directory~~ N/A - using pure C++ map widget
- [ ] Create src/resources/ directory
- [ ] Copy SVG markers from Flutter project
- [ ] Copy resource_types.txt config

---

## Phase 2: Data Models

### Enums
- [x] Gender.h (enum class + string conversion)
- [x] ViewMode.h (EQ/RS/Emergency/Tags)
- [x] MarkerDecorationType.h (None/Med/Rec/Com/Spec)
- [x] ResourceLevel.h (Person/Family)

### Core Models
- [x] Person.h/cpp
  - [x] All properties (id, name, phone, email, gender, birthDate, callings)
  - [x] withX() immutable update methods
  - [x] toJson() / fromJson()
  - [x] operator== / operator!=
  - [ ] Unit tests

- [x] Family.h/cpp
  - [x] All properties (id, name, lat, lng, address, members, visits)
  - [x] withX() immutable update methods
  - [x] toJson() / fromJson()
  - [ ] Unit tests

- [x] MinisteringVisit.h/cpp
  - [x] Properties (contactDate, resultNotes, futureActions, timestamp)
  - [x] toJson() / fromJson()
  - [ ] Unit tests

- [x] Team.h/cpp
  - [x] Properties (id, name, color, leaderId, memberIds)
  - [x] withX() methods
  - [x] toJson() / fromJson()
  - [ ] Unit tests

- [x] Tag.h/cpp
  - [x] Properties (id, name, personIds, familyIds)
  - [x] withX() methods
  - [x] toJson() / fromJson()
  - [ ] Unit tests

- [x] ResourceCategory.h/cpp
  - [x] Properties (id, name, level, sortOrder, markerDecorationType)
  - [x] toJson() / fromJson()
  - [ ] Unit tests

- [x] ResourceType.h/cpp
  - [x] Properties (id, name, categoryId, isCustom, personIds, familyIds)
  - [x] toJson() / fromJson()
  - [ ] Unit tests

- [x] No MinisteringGroupBase - avoided inheritance for cleaner value semantics
  - [x] EQ and RS groups are independent classes

- [x] EQMinisteringGroup.h/cpp
  - [x] familyIds, ministerIds, interviewedDate
  - [x] toJson() / fromJson()
  - [ ] Unit tests

- [x] RSMinisteringGroup.h/cpp
  - [x] ministeredPersonIds, ministerIds, interviewedDate
  - [x] toJson() / fromJson()
  - [ ] Unit tests

- [x] MinisteringDistrict.h/cpp
  - [x] Properties (id, name, presidencyMemberId, groupIds)
  - [x] toJson() / fromJson()
  - [ ] Unit tests

- [x] Document.h/cpp
  - [x] All collections (families, teams, tags, categories, resourceTypes)
  - [x] EQ/RS districts and groups (current + proposed)
  - [x] withX() methods for each collection
  - [x] toJson() / fromJson()
  - [x] Document::empty() factory
  - [ ] Unit tests

- [x] Ward.h/cpp - Ward metadata (name, unit number, chapel info)
- [x] Stake.h/cpp - Stake metadata

### Model Adapters (QAbstractItemModel)
- [x] FamilyListModel.h/cpp
  - [x] All roles defined
  - [x] roleNames() for QML
  - [x] Constructor takes DocumentManager* and Filter*
  - [x] Auto-rebuilds on document/filter changes
  - [ ] Unit tests

- [x] PersonListModel.h/cpp
  - [x] All roles (including FamilyIdRole)
  - [x] roleNames()
  - [x] Constructor takes DocumentManager* and Filter*
  - [x] Auto-rebuilds on document/filter changes
  - [ ] Unit tests

- [x] Filter.h/cpp (in src/listmodels/)
  - [x] Search text filtering
  - [x] passes(Document, Family) method
  - [x] passes(Document, Person) method
  - [x] changed() signal for model integration

- [x] TeamListModel.h/cpp
  - [ ] Unit tests

- [x] TagListModel.h/cpp
  - [ ] Unit tests

- [ ] DistrictTreeModel.h/cpp (for EQ/RS hierarchy)
  - [ ] Tree structure support
  - [ ] Unit tests

- [ ] ResourceTreeModel.h/cpp
  - [ ] Category > Type hierarchy
  - [ ] Unit tests

---

## Phase 3: Command Pattern

### Base Infrastructure
- [x] Command.h (abstract interface)
- [x] CommandHistory.h/cpp
  - [x] execute() method
  - [x] undo() / redo() methods
  - [x] canUndo() / canRedo() properties
  - [x] markSaved() / isDirty()
  - [x] Q_PROPERTY declarations
  - [ ] Unit tests

### Family Commands
- [x] AddFamilyCommand
- [x] UpdateFamilyCommand
- [x] DeleteFamilyCommand
- [ ] Unit tests for all

### Team Commands
- [x] AddTeamCommand
- [x] UpdateTeamCommand
- [x] DeleteTeamCommand
- [x] AddTeamMemberCommand
- [x] RemoveTeamMemberCommand
- [ ] Unit tests for all

### Ministering Commands
- [x] AddEQDistrictCommand / AddRSDistrictCommand
- [x] UpdateDistrictCommand
- [x] DeleteDistrictCommand
- [x] AddEQGroupCommand / AddRSGroupCommand
- [x] UpdateEQGroupCommand / UpdateRSGroupCommand
- [x] DeleteGroupCommand
- [x] SetGroupInterviewedCommand
- [x] AddVisitCommand (EQ and RS variants)
- [ ] Unit tests for all

### Tag Commands
- [x] CreateTagCommand
- [x] UpdateTagCommand
- [x] DeleteTagCommand
- [x] AssignTagToPersonCommand
- [x] AssignTagToFamilyCommand
- [x] UnassignTagCommand
- [ ] Unit tests for all

### Resource Commands
- [x] CreateResourceTypeCommand
- [x] UpdateResourceTypeCommand
- [x] DeleteResourceTypeCommand
- [x] AssignResourceToPersonCommand
- [x] AssignResourceToFamilyCommand
- [x] UnassignResourceCommand
- [x] UpdateCategoryCommand
- [ ] Unit tests for all

### Import Commands
- [ ] ImportJsonCommand
- [ ] ImportWardDirectoryPdfCommand
- [ ] ImportMinisteringPdfCommand
- [ ] Unit tests

---

## Phase 4: Services

### Document Management
- [x] DocumentManager.h/cpp
  - [x] Owns Document and CommandHistory
  - [x] executeCommand() method
  - [x] File I/O (new, open, save, saveAs)
  - [x] Q_PROPERTY for document access
  - [x] Signals for changes
  - [ ] Integration tests

### JSON Service
- [x] JsonService.h/cpp
  - [x] saveDocument() / loadDocument()
  - [x] Version handling
  - [ ] Migration logic (v1 → v2 → v3)
  - [x] toJson() / fromJson() for Document
  - [ ] Unit tests with sample files

### Filter Service
- [x] FilterService.h/cpp
  - [x] Filters struct definition
  - [x] filter() main method
  - [x] matchesSearch()
  - [x] matchesTeam()
  - [x] matchesSkills()
  - [x] matchesTags()
  - [x] matchesGender()
  - [x] matchesAge()
  - [ ] Unit tests

### Statistics Service
- [ ] StatisticsService.h/cpp
  - [ ] calculateFamilyStats()
  - [ ] calculateTeamStats()
  - [ ] calculateMinisteringStats()
  - [ ] Unit tests

### Resource Service
- [ ] ResourceService.h/cpp
  - [ ] loadDefaultConfig()
  - [ ] parseConfigFile()
  - [ ] getResourcesForPerson()
  - [ ] getResourcesForFamily()
  - [ ] Unit tests

### Search Service
- [ ] SearchService.h/cpp
  - [ ] searchFamilies()
  - [ ] searchPersons()
  - [ ] Full-text matching
  - [ ] Unit tests

### Geocoding Service
- [x] GeocodingService.h/cpp
  - [x] Nominatim API integration
  - [x] Async request handling (QNetworkAccessManager)
  - [x] Rate limiting (1.1s between requests)
  - [x] Fail silently on errors
- [x] BackgroundGeocodingService.h/cpp
  - [x] Queue-based processing
  - [x] Progress signals (progressUpdated, familyGeocoded, finished)
  - [x] Duplicate detection
  - [x] Cancellation support (stop())
- [x] DocumentManager integration
  - [x] startBatchGeocoding() for imports
  - [x] queueFamilyForGeocoding() for address changes
  - [x] Silent updates (no undo/redo)
- [x] MainWindow integration
  - [x] StatusBar progress display
  - [x] Triggers after PDF imports
- [ ] Unit tests (mocked network)

### Preferences Service
- [ ] PreferencesService.h/cpp
  - [ ] QSettings wrapper
  - [ ] lastFilePath, lastViewMode
  - [ ] Window geometry
  - [ ] Unit tests

### Tile Cache Service
- [ ] TileCacheService.h/cpp
  - [ ] Cache directory management
  - [ ] hasTile() / getTilePath()
  - [ ] Async tile download
  - [ ] cacheRegion() for bulk download
  - [ ] Cache size management
  - [ ] Unit tests

### PDF Import Service
- [x] PdfExtractor.h/cpp (MuPDF wrapper for text extraction)
  - [x] Character and run extraction
  - [x] Font size and position info
  - [x] Classification rule support for typing runs
- [x] WardDirectoryPdfParser.h/cpp (Ward directory PDF parsing)
  - [x] Run type classification by position/font
  - [x] Family block detection
  - [x] Person/child parsing
  - [x] Address and coordinate parsing
  - [x] Phone/email/calling extraction
  - [x] Fail-fast error handling for unexpected formats
- [x] PersonMatching.h/cpp (Name comparison utilities)
  - [x] Surname extraction from family names
  - [x] Name normalization and comparison
- [x] WardDirectoryImportService.h/cpp (Import orchestration)
  - [x] Uses WardDirectoryPdfParser for parsing
  - [x] Converts parsed data to Family objects
  - [x] Progress callback support
  - [ ] Integration tests with sample PDFs

### Unit Lookup Service
- [x] UnitLookupService.h/cpp
  - [x] Ward lookup from maps.churchofjesuschrist.org
  - [x] Stake lookup with ward unit numbers
  - [x] HTML parsing (Next.js rendered pages)
  - [x] Extract name, address, coordinates, phone, meeting time

### Ministering PDF Import
- [x] EQMinisteringPdfParser.h/cpp
  - [x] Field classification by position/font/bold
  - [x] Column partitioning for Y-sorted interleaved fields
  - [x] District/companionship/minister/family parsing
  - [x] Page break handling with iterator
- [x] MinisteringImportService.h/cpp
  - [x] EQ ministering PDF import
  - [x] District and group creation
  - [x] Minister matching by name
  - [x] Family creation for unmatched families
  - [x] RS ministering PDF import (unified parser auto-detects format)
  - [ ] Integration tests

### PDF Report Service
- [ ] PdfReportService.h/cpp
  - [ ] QPdfWriter / QPainter setup
  - [ ] Header/footer rendering
  - [ ] Emergency resource report
  - [ ] Tag report
  - [ ] Skills/equipment report
  - [ ] Integration tests

---

## Phase 5: ViewModels

### Document ViewModel
- [ ] DocumentViewModel.h/cpp
  - [ ] document() accessor
  - [ ] File operations (slots)
  - [ ] Undo/redo (slots)
  - [ ] Q_PROPERTY declarations
  - [ ] Change signals
  - [ ] Integration tests

### Family ViewModel
- [ ] FamilyViewModel.h/cpp
  - [ ] Current family for editing
  - [ ] Validation logic
  - [ ] Save/cancel actions
  - [ ] Tests

### Filter ViewModel
- [ ] FilterViewModel.h/cpp
  - [ ] All filter properties
  - [ ] Toggle methods
  - [ ] Clear method
  - [ ] toFilters() conversion
  - [ ] Tests

### Map ViewModel
- [ ] MapViewModel.h/cpp
  - [ ] Highlight management
  - [ ] Selected family
  - [ ] Center-on actions
  - [ ] Layer switching
  - [ ] Tests

### Ministering ViewModel
- [ ] MinisteringViewModel.h/cpp
  - [ ] Current/Proposed toggle
  - [ ] Selected district/group
  - [ ] EQ vs RS mode
  - [ ] Visit recording
  - [ ] Tests

### Team ViewModel
- [ ] TeamViewModel.h/cpp
  - [ ] Team CRUD
  - [ ] Member management
  - [ ] Tests

### Tag ViewModel
- [ ] TagViewModel.h/cpp
  - [ ] Tag CRUD
  - [ ] Assignment management
  - [ ] Tests

### Resource ViewModel
- [ ] ResourceViewModel.h/cpp
  - [ ] Category/type selection
  - [ ] Resource discovery
  - [ ] Tests

### Highlight Orchestrator
- [ ] HighlightOrchestrator.h/cpp
  - [ ] View mode awareness
  - [ ] Active tab tracking
  - [ ] Provider selection
  - [ ] Tests

---

## Phase 6: Core UI Widgets

### Main Window
- [x] MainWindow.h/cpp
  - [x] QMainWindow setup
  - [x] Menu bar (File, Edit, View, Help)
  - [ ] Toolbar (optional)
  - [x] Status bar
  - [x] Central splitter layout
  - [x] Keyboard shortcuts
  - [x] Close event handling (save prompt)
  - [ ] Window geometry persistence

### Sidebar
- [ ] Sidebar.h/cpp
  - [ ] Tab widget for navigation
  - [ ] Ward List tab
  - [ ] View mode tabs (EQ/RS/Emergency/Tags)
  - [ ] Tab switching signals

### Ward List View
- [x] WardListView.h/cpp
  - [x] FilterableListWidget with FamilyListModel
  - [x] Owns its own Filter and FamilyListModel
  - [x] Search field integration (via FilterableListWidget)
  - [x] Selection handling (familySelected signal)
  - [x] visibleFamiliesChanged signal for map filtering
  - [ ] Filter chips
  - [ ] Context menu
  - [ ] Double-click to edit

- [x] WardListDialog.h/cpp
  - [x] Modal dialog for selecting families or persons
  - [x] FamilyMode and PersonMode
  - [x] SingleSelect and MultiSelect modes
  - [x] Embedded MapWidget with split view
  - [x] Static convenience methods (selectFamily, selectFamilies, selectPerson, selectPersons)
  - [x] Map click selects list item
  - [x] List selection centers map

### Shared Widgets
- [x] SearchField.h/cpp
  - [x] QLineEdit with clear button
  - [x] searchTextChanged signal
  - [x] Placeholder text

- [x] FilterableListWidget.h/cpp
  - [x] SearchField + QListView combo
  - [x] setModel() / setIdRole()
  - [x] SingleSelect / MultiSelect modes
  - [x] setSelectedIds() / selectedIds()
  - [x] selectionChanged signal

- [ ] FilterChips.h/cpp
  - [ ] Horizontal chip layout
  - [ ] Toggle behavior
  - [ ] Clear all button

- [ ] FamilyTile.h/cpp
  - [ ] Family display widget
  - [ ] Name, address, member count
  - [ ] Team color indicator
  - [ ] Resource badges

- [ ] PersonTile.h/cpp
  - [ ] Person display widget
  - [ ] Name, phone, callings

- [ ] BadgeWidget.h/cpp
  - [ ] Resource/skill badges
  - [ ] Color-coded display

---

## Phase 7: Map Integration

### Pure C++ Implementation (replaced QML approach)
- [x] MapWidget.h/cpp - Pure QWidget with QPainter rendering
  - [x] Web Mercator tile rendering (256x256 tiles)
  - [x] Mouse drag panning
  - [x] Mouse wheel zoom (centered on cursor)
  - [x] +/- zoom buttons
  - [x] Recenter button (fit all families)
  - [x] Street/Satellite layer toggle
  - [x] Marker rendering with selection/highlighting
  - [x] centerOnFamily() method
  - [x] fitAllFamilies() method
  - [x] setVisibleFamilyIds() for filter-based de-emphasis
  - [x] familyClicked signal
  - [x] Dynamic attribution from tile providers

### Tile Services
- [x] TileService.h/cpp - Main tile orchestration
  - [x] Provider fallback (OSM → ESRI, USGS → ESRI)
  - [x] Scaled tile generation for missing zoom levels
  - [x] Dynamic attribution tracking (tracks which providers contributed)
  - [x] Layer switching (street/satellite)
- [x] TileFetchService.h/cpp - Network fetching
  - [x] Multi-provider configuration
  - [x] Automatic fallback on failure
  - [x] User-Agent and headers
  - [x] providerAttribution() lookup
- [x] TileCacheService.h/cpp - SQLite disk cache
  - [x] Tile storage with metadata (provider, fetch date, etag)
  - [x] Stale tile detection
  - [x] isScaledUp flag for quality tracking

### Not implemented (QML approach abandoned)
- ~~QML Setup~~ N/A
- ~~MapView.qml~~ N/A
- ~~MarkerItem.qml~~ N/A

---

## Phase 8: Dialogs

### Family Dialog
- [ ] FamilyDialog.h/cpp
  - [ ] Form layout
  - [ ] Name field
  - [ ] Address field with geocode button
  - [ ] Latitude/longitude spinboxes
  - [ ] Members list
  - [ ] Add/remove member buttons
  - [ ] OK/Cancel buttons
  - [ ] Validation
  - [ ] Result accessor

### Person Form
- [ ] PersonForm.h/cpp
  - [ ] Name field
  - [ ] Phone fields (primary, alt)
  - [ ] Email field
  - [ ] Gender selector
  - [ ] Birth date (year/month/day)
  - [ ] Callings list
  - [ ] Add/remove calling

### Visit Dialog
- [ ] VisitDialog.h/cpp
  - [ ] Contact date picker
  - [ ] Result notes text
  - [ ] Future actions text
  - [ ] OK/Cancel

### Statistics Dialog
- [ ] StatisticsDialog.h/cpp
  - [ ] Family count
  - [ ] Person count
  - [ ] Team statistics
  - [ ] Ministering coverage
  - [ ] Resource distribution

### Tag Dialog
- [ ] TagDialog.h/cpp
  - [ ] Tag name field
  - [ ] Assigned items list
  - [ ] Search/filter for assignment
  - [ ] OK/Cancel

### Resource Dialog
- [ ] ResourceDialog.h/cpp
  - [ ] Resource name
  - [ ] Category selection
  - [ ] Custom flag
  - [ ] OK/Cancel

---

## Phase 9: Advanced Views

### Ministering Sidebar
- [ ] MinisteringSidebar.h/cpp
  - [ ] Current/Proposed toggle
  - [ ] District list
  - [ ] Group list per district
  - [ ] Group detail panel
  - [ ] Minister list
  - [ ] Assignment list (families or persons)
  - [ ] Visit status indicators
  - [ ] Interview date display
  - [ ] Add/edit/delete actions
  - [ ] Visit recording button

### Emergency View
- [ ] EmergencyView.h/cpp
  - [ ] Team list
  - [ ] Team member display
  - [ ] Leader indicator
  - [ ] Resource summary
  - [ ] Add/edit/delete team
  - [ ] Assign members

### Teams View
- [ ] TeamsView.h/cpp
  - [ ] Full team management
  - [ ] Color picker
  - [ ] Member drag/drop (optional)

### Resource View
- [ ] ResourceView.h/cpp
  - [ ] Category tree
  - [ ] Resource type list
  - [ ] Assignment counts
  - [ ] Discovery features
  - [ ] Add custom type

### Tags View
- [ ] TagsView.h/cpp
  - [ ] Tag list
  - [ ] Assignment counts
  - [ ] Add/edit/delete tag
  - [ ] Bulk assignment

### Selection View
- [ ] SelectionView.h/cpp
  - [ ] Modal-free picker
  - [ ] Person or Family mode
  - [ ] Search/filter
  - [ ] Multi-select support
  - [ ] Done/Cancel actions

---

## Phase 10: PDF Features

### PDF Import
- [ ] Complete ward directory import
- [ ] Complete ministering import
- [ ] Progress feedback
- [ ] Error reporting
- [ ] Conflict resolution UI

### PDF Reports
- [ ] Emergency resource report
- [ ] Tag-based report
- [ ] Skills report
- [ ] Equipment report
- [ ] Report preview (optional)

---

## Phase 11: Advanced Features

### Background Geocoding
- [x] Implemented via BackgroundGeocodingService (not QThread)
- [x] Uses async QNetworkAccessManager with timer-based rate limiting
- [x] Progress signals
- [x] Cancellation support
- [x] Integration with DocumentManager

### Keyboard Shortcuts
- [ ] Ctrl+N: New
- [ ] Ctrl+O: Open
- [ ] Ctrl+S: Save
- [ ] Ctrl+Shift+S: Save As
- [ ] Ctrl+Z: Undo
- [ ] Ctrl+Y: Redo
- [ ] Ctrl+F: Focus search
- [ ] 1-4: Switch view modes
- [ ] Delete: Delete selected

### Context Menus
- [ ] Family context menu (edit, delete, center on map)
- [ ] Person context menu
- [ ] Team context menu
- [ ] Tag context menu

---

## Phase 12: Polish

### Window State
- [ ] Save/restore window geometry
- [ ] Save/restore splitter positions
- [ ] Remember last file
- [ ] Remember last view mode

### Error Handling
- [ ] User-friendly error messages
- [ ] File error handling
- [ ] Network error handling
- [ ] Validation feedback

### Performance
- [ ] Profile large ward data
- [ ] Optimize model updates
- [ ] Lazy loading where needed
- [ ] Memory usage check

### Accessibility
- [ ] Keyboard navigation
- [ ] Screen reader labels
- [ ] High contrast support

---

## Phase 13: Testing

### Unit Tests
- [ ] All model tests passing
- [ ] All command tests passing
- [ ] All service tests passing
- [ ] ViewModel tests passing

### Integration Tests
- [ ] File I/O round-trip
- [ ] Command execution flow
- [ ] PDF import/export

### Manual Testing
- [ ] Fresh install test
- [ ] Large data set test
- [ ] All features walkthrough
- [ ] Cross-platform verification (if applicable)

---

## Phase 14: Packaging

### Windows
- [ ] Configure NSIS installer OR WiX
- [ ] Bundle Qt DLLs
- [ ] Application icon
- [ ] File association (.wardplan)
- [ ] Start menu entry
- [ ] Test installer

### macOS (if needed)
- [ ] Configure bundle
- [ ] Info.plist
- [ ] Icon file
- [ ] Code signing (optional)
- [ ] DMG creation

### Linux (if needed)
- [ ] AppImage configuration
- [ ] Desktop file
- [ ] Icon installation
- [ ] Test AppImage

---

## Phase 15: Documentation

- [ ] Update ARCHITECTURE.md
- [ ] Create BUILDING.md (build instructions)
- [ ] Create USER_GUIDE.md
- [ ] API documentation (Doxygen optional)
- [ ] README.md for repository

---

## Progress Summary

| Phase | Status | Completion |
|-------|--------|------------|
| 1. Foundation | Complete | 85% |
| 2. Data Models | In Progress | 90% |
| 3. Commands | In Progress | 90% |
| 4. Services | In Progress | 75% |
| 5. ViewModels | In Progress | 10% |
| 6. Core UI | In Progress | 45% |
| 7. Map | Complete | 100% |
| 8. Dialogs | Not Started | 0% |
| 9. Advanced Views | Not Started | 0% |
| 10. PDF | In Progress | 60% |
| 11. Advanced Features | In Progress | 20% |
| 12. Polish | Not Started | 0% |
| 13. Testing | Not Started | 5% |
| 14. Packaging | Not Started | 0% |
| 15. Documentation | In Progress | 65% |

**Overall Progress: ~49%**

---

## Notes

- Check off items as they are completed
- Add notes for any blockers or issues
- Update progress percentages weekly
- Prioritize based on dependencies (earlier phases must complete first)

---

## TODO: Code Audits

### tr() Audit
Review existing code to wrap user-visible strings with `tr()`:
- [ ] Menu items, button labels, dialog titles
- [ ] Error messages, status messages
- [ ] Placeholder text, tooltips
- [ ] Any string shown to the user

### Coding Style Audit
Review existing code for coding style compliance (see CODING_STYLE.md):
- [ ] Allman-style braces
- [ ] No auto except for iterators
- [ ] Leading operators on continuation lines
- [ ] Filename-only includes
- [ ] No complex lambdas
