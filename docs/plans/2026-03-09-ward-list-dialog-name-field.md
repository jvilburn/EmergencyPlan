# WardListDialog Name Field Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** Add an editable name field to WardListDialog so callers can edit both name and selection in one dialog, eliminating separate QInputDialog name prompts.

**Architecture:** WardListDialog gains a `QLineEdit` name field at the top, always visible in multi-select mode. The static `selectPersons`/`selectFamilies` methods accept a `nameLabel` param (e.g. "Team", "Need") and return a result struct containing both the name and selected IDs. Callers (TeamsView, EmergencyAssetView, NeedsSubView) are updated to use the unified dialog for both Add and Edit operations, removing separate QInputDialog calls.

**Tech Stack:** Qt 6 / C++17, QLineEdit, QLabel, QHBoxLayout

---

### Task 1: Add name field to WardListDialog

**Files:**
- Modify: `src/widgets/WardListDialog.h`
- Modify: `src/widgets/WardListDialog.cpp`

**Step 1:** Add the name field UI and accessor to the header.

In `WardListDialog.h`, add forward declaration for `QLineEdit` and `QLabel`, add a `name()` accessor, and add member variables `m_nameLabel` and `m_nameEdit`:

```cpp
// Forward declarations (add QLabel and QLineEdit)
class QLabel;
class QLineEdit;

// Public accessor:
QString name() const;

// Private members (add alongside existing members):
QLabel* m_nameLabel = nullptr;
QLineEdit* m_nameEdit = nullptr;
```

**Step 2:** In `WardListDialog.cpp`, add `#include <QLineEdit>`, `#include <QLabel>`, and `#include <QHBoxLayout>`. In `setupUi()`, always add the name field row above the FilterBar (every caller needs it):

```cpp
// After mainLayout is created, before FilterBar:
QHBoxLayout* nameLayout = new QHBoxLayout();
m_nameLabel = new QLabel(this);
m_nameEdit = new QLineEdit(this);
nameLayout->addWidget(m_nameLabel);
nameLayout->addWidget(m_nameEdit, 1);
mainLayout->addLayout(nameLayout);
```

**Step 3:** Implement the `name()` accessor:

```cpp
QString WardListDialog::name() const
{
    return m_nameEdit->text().trimmed();
}
```

**Step 4:** Build and verify the app compiles.

**Step 5:** Commit.

```
feat: add name field UI to WardListDialog
```

---

### Task 2: Update selectPersons/selectFamilies return types and signatures

**Files:**
- Modify: `src/widgets/WardListDialog.h`
- Modify: `src/widgets/WardListDialog.cpp`

**Step 1:** In `WardListDialog.h`, add result structs before the class definition:

```cpp
/// Result from multi-select person dialog.
struct PersonSelectionResult
{
    QString name;
    QList<PersonId> personIds;
};

/// Result from multi-select family dialog.
struct FamilySelectionResult
{
    QString name;
    QList<FamilyId> familyIds;
};
```

**Step 2:** Update the static method signatures. The `title` parameter becomes `nameLabel` (the label text for the name field), and a new `initialName` parameter is added. The window title is set from the `nameLabel` instead of being a separate param:

Old:
```cpp
static std::optional<QList<PersonId>> selectPersons(
    DocumentManager* documentManager,
    const QString& title,
    const QList<PersonId>& initialIds = {},
    QWidget* parent = nullptr);

static std::optional<QList<FamilyId>> selectFamilies(
    DocumentManager* documentManager,
    const QString& title,
    const QList<FamilyId>& initialIds = {},
    QWidget* parent = nullptr);
```

New:
```cpp
static std::optional<PersonSelectionResult> selectPersons(
    DocumentManager* documentManager,
    const QString& nameLabel,
    const QString& initialName,
    const QList<PersonId>& initialIds = {},
    QWidget* parent = nullptr);

static std::optional<FamilySelectionResult> selectFamilies(
    DocumentManager* documentManager,
    const QString& nameLabel,
    const QString& initialName,
    const QList<FamilyId>& initialIds = {},
    QWidget* parent = nullptr);
```

**Step 3:** Update the implementations in `WardListDialog.cpp`. Key changes to `selectPersons`:

```cpp
std::optional<PersonSelectionResult> WardListDialog::selectPersons(
    DocumentManager* documentManager,
    const QString& nameLabel,
    const QString& initialName,
    const QList<PersonId>& initialIds,
    QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, true, parent);
    dialog.setWindowTitle(tr("Select People — %1").arg(nameLabel));
    dialog.m_nameLabel->setText(nameLabel + tr(":"));
    dialog.m_nameEdit->setText(initialName);
    connect(dialog.m_personModel, &PersonTreeModel::dataChanged,
            dialog.m_mapWidget, &MapWidget::updateHighlights);
    if (!initialIds.isEmpty())
    {
        QSet<PersonId> idSet(initialIds.begin(), initialIds.end());
        dialog.m_personModel->setCheckedPersonIds(idSet);

        // Scroll to first checked person
        QModelIndex firstIndex = dialog.m_personModel->indexForPersonId(initialIds.first());
        if (firstIndex.isValid())
        {
            dialog.m_treeView->scrollTo(firstIndex);
        }
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    QSet<PersonId> checked = dialog.m_personModel->checkedPersonIds();
    PersonSelectionResult result;
    result.name = dialog.name();
    result.personIds = QList<PersonId>(checked.begin(), checked.end());
    return result;
}
```

Apply the same pattern to `selectFamilies`, using `FamilySelectionResult` and `m_familyModel->checkedFamilyIds()`.

**Step 4:** Build and verify the app compiles (callers will break — that's expected, we fix them next).

**Step 5:** Commit.

```
feat: update selectPersons/selectFamilies to return name with selection
```

---

### Task 3: Update TeamsView to use unified dialog

**Files:**
- Modify: `src/widgets/TeamsView.h`
- Modify: `src/widgets/TeamsView.cpp`

**Step 1:** In `TeamsView.h`, remove the `editTeam()` declaration and replace `showSelectMembersDialog` with `showTeamDialog`:

```cpp
// Remove:
void editTeam();
void showSelectMembersDialog(const TeamId& teamId);

// Add:
void showTeamDialog(const std::optional<TeamId>& teamId);
```

**Step 2:** In `TeamsView.cpp`, replace the `addTeam()` method to call `showTeamDialog(std::nullopt)`:

```cpp
void TeamsView::addTeam()
{
    showTeamDialog(std::nullopt);
}
```

**Step 3:** Remove the old `editTeam()` method entirely.

**Step 4:** In `TeamsView.h`, replace `void editTeam();` with `void editSelectedTeam();`. In `TeamsView.cpp`, implement:

```cpp
void TeamsView::editSelectedTeam()
{
    std::optional<TeamId> teamId = selectedTeamId();
    if (teamId)
    {
        showTeamDialog(teamId);
    }
}
```

In the constructor, find the `m_editButton` connection and change it from `editTeam` to `editSelectedTeam`:

```cpp
connect(m_editButton, &QPushButton::clicked, this, &TeamsView::editSelectedTeam);
```

**Step 5:** Update `onTreeDoubleClicked` to call `editSelectedTeam()` instead of `editTeam()`:

```cpp
case ItemType::Team:
    editSelectedTeam();
    break;
```

**Step 6:** Implement `showTeamDialog`. This replaces both `addTeam()`'s QInputDialog and `showSelectMembersDialog`:

```cpp
void TeamsView::showTeamDialog(const std::optional<TeamId>& teamId)
{
    QString initialName;
    QList<PersonId> initialIds;

    if (teamId)
    {
        const Document& doc = m_documentManager->document();
        std::optional<Team> teamOpt = doc.findTeamById(*teamId);
        if (!teamOpt)
        {
            return;
        }
        initialName = teamOpt->name();
        initialIds = teamOpt->memberIds().values();
    }

    std::optional<PersonSelectionResult> result = WardListDialog::selectPersons(
        m_documentManager, tr("Team"), initialName, initialIds, this);

    if (!result || result->name.isEmpty())
    {
        return;
    }

    QSet<PersonId> newMembers(result->personIds.begin(), result->personIds.end());

    if (teamId)
    {
        // Edit existing team
        const Document& doc = m_documentManager->document();
        std::optional<Team> teamOpt = doc.findTeamById(*teamId);
        if (!teamOpt)
        {
            return;
        }

        Team updated = *teamOpt;
        updated.setName(result->name);
        updated.setMemberIds(newMembers);

        // Clear leader if they were removed
        if (updated.leaderId() && !newMembers.contains(*updated.leaderId()))
        {
            updated.setLeaderId(std::nullopt);
        }

        if (updated != *teamOpt)
        {
            m_documentManager->executeCommand(
                std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
        }
    }
    else
    {
        // Create new team
        Team team = Team::create(result->name);
        team.setMemberIds(newMembers);
        m_documentManager->executeCommand(
            std::make_unique<AddTeamCommand>(team));
    }
}
```

**Step 7:** Update `selectMembersFromContextMenu` to call `showTeamDialog`:

```cpp
void TeamsView::selectMembersFromContextMenu()
{
    std::optional<TeamId> teamId = m_contextTeamId;
    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (teamId)
    {
        showTeamDialog(teamId);
    }
}
```

**Step 8:** Update the context menu. Change "Select Members..." to "Edit..." and remove the "Rename..." action:

```cpp
case ItemType::Team:
{
    m_contextTeamId = m_model->teamIdAt(index);
    if (m_contextTeamId)
    {
        menu.addAction(tr("Edit..."), this, &TeamsView::selectMembersFromContextMenu);
        menu.addSeparator();
        menu.addAction(tr("Delete"), this, &TeamsView::deleteTeam);
    }
    break;
}
```

**Step 9:** Build and verify the app compiles.

**Step 10:** Commit.

```
feat: unify TeamsView add/edit into single WardListDialog
```

---

### Task 4: Update EmergencyAssetView to use unified dialog

**Files:**
- Modify: `src/widgets/EmergencyAssetView.h`
- Modify: `src/widgets/EmergencyAssetView.cpp`

The EmergencyAssetView has three instances (Medical, Communications, Recovery), each with a different name label. The label should be derived from the `ResponseArea`.

**Step 1:** In `EmergencyAssetView.h`, remove `editAsset()` and `showSelectPeopleDialog()`, add `showAssetDialog()` and `editSelectedAsset()`:

```cpp
// Remove:
void editAsset();
void showSelectPeopleDialog(const EmergencyAssetId& assetId);

// Add:
void editSelectedAsset();
void showAssetDialog(const std::optional<EmergencyAssetId>& assetId);
QString nameLabel() const;
```

**Step 2:** Implement `nameLabel()` which returns the label text based on the model's area:

```cpp
QString EmergencyAssetView::nameLabel() const
{
    switch (m_model->area())
    {
    case ResponseArea::Medical:
        return tr("Medical skill");
    case ResponseArea::Communications:
        return tr("Communication skill/gear");
    case ResponseArea::Recovery:
        return tr("Skill or Gear");
    case ResponseArea::None:
    case ResponseArea::SpecialNeeds:
        Q_UNREACHABLE();
        return tr("Name");
    }
    Q_UNREACHABLE();
    return tr("Name");
}
```

**Step 3:** Replace `addAsset()` to call `showAssetDialog(std::nullopt)`:

```cpp
void EmergencyAssetView::addAsset()
{
    showAssetDialog(std::nullopt);
}
```

**Step 4:** Replace `editAsset()` with `editSelectedAsset()`:

```cpp
void EmergencyAssetView::editSelectedAsset()
{
    std::optional<EmergencyAssetId> assetId = selectedAssetId();
    if (assetId)
    {
        showAssetDialog(assetId);
    }
}
```

In the constructor, find the `m_editButton` connection and change it from `editAsset` to `editSelectedAsset`:

```cpp
connect(m_editButton, &QPushButton::clicked, this, &EmergencyAssetView::editSelectedAsset);
```

Also update `onTreeDoubleClicked` to call `editSelectedAsset()` instead of `editAsset()`.

**Step 5:** Implement `showAssetDialog`:

```cpp
void EmergencyAssetView::showAssetDialog(const std::optional<EmergencyAssetId>& assetId)
{
    QString initialName;
    QList<PersonId> initialIds;

    if (assetId)
    {
        const Document& doc = m_documentManager->document();
        std::optional<EmergencyAsset> assetOpt = doc.findEmergencyAssetById(*assetId);
        if (!assetOpt)
        {
            return;
        }
        initialName = assetOpt->name();
        initialIds = assetOpt->personIds().values();
    }

    std::optional<PersonSelectionResult> result = WardListDialog::selectPersons(
        m_documentManager, nameLabel(), initialName, initialIds, this);

    if (!result || result->name.isEmpty())
    {
        return;
    }

    QSet<PersonId> newPersons(result->personIds.begin(), result->personIds.end());

    if (assetId)
    {
        // Edit existing asset
        const Document& doc = m_documentManager->document();
        std::optional<EmergencyAsset> assetOpt = doc.findEmergencyAssetById(*assetId);
        if (!assetOpt)
        {
            return;
        }

        EmergencyAsset updated = *assetOpt;
        updated.setName(result->name);
        updated.setPersonIds(newPersons);

        if (updated != *assetOpt)
        {
            m_documentManager->executeCommand(
                std::make_unique<UpdateEmergencyAssetCommand>(*assetOpt, updated));
        }
    }
    else
    {
        // Create new asset
        EmergencyAsset asset = EmergencyAsset::create(result->name, m_model->area());
        asset.setPersonIds(newPersons);
        m_documentManager->executeCommand(
            std::make_unique<AddEmergencyAssetCommand>(asset));
    }
}
```

**Step 6:** Update `selectPeopleFromContextMenu` to call `showAssetDialog`:

```cpp
void EmergencyAssetView::selectPeopleFromContextMenu()
{
    std::optional<EmergencyAssetId> assetId = m_contextAssetId;
    m_contextAssetId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (assetId)
    {
        showAssetDialog(assetId);
    }
}
```

**Step 7:** Update the context menu. Change "Select People..." to "Edit..." and remove the "Rename..." action:

```cpp
case ItemType::Asset:
{
    m_contextAssetId = m_model->assetIdAt(index);
    menu.addAction(tr("Edit..."), this, &EmergencyAssetView::selectPeopleFromContextMenu);
    menu.addSeparator();
    menu.addAction(tr("Delete"), this, &EmergencyAssetView::deleteAsset);
}
break;
```

**Step 8:** Build and verify the app compiles.

**Step 9:** Commit.

```
feat: unify EmergencyAssetView add/edit into single WardListDialog
```

---

### Task 5: Update NeedsSubView to use WardListDialog with name field

**Files:**
- Modify: `src/widgets/NeedsSubView.h`
- Modify: `src/widgets/NeedsSubView.cpp`

NeedsSubView is different from Teams/Assets: it uses **single-select** (pick one person) with a name field (the need note). The current `showAddNeedDialog` builds a custom QDialog with a "Select..." button and a note field. This should be replaced with `selectPerson` gaining name field support.

**Step 1:** First, add a single-select variant with name field to WardListDialog. In `WardListDialog.h`:

```cpp
/// Show dialog to select a single person with a name field.
/// Returns nullopt if cancelled.
static std::optional<PersonSelectionResult> selectPersonWithName(
    DocumentManager* documentManager,
    const QString& nameLabel,
    const QString& initialName = {},
    const std::optional<PersonId>& initialId = std::nullopt,
    QWidget* parent = nullptr);
```

**Step 2:** The name field is already always created unconditionally (Task 1 creates it for every dialog instance). The static methods just need to set the label text and initial value. No hide/show logic needed.

**Step 3:** Implement `selectPersonWithName`:

```cpp
std::optional<PersonSelectionResult> WardListDialog::selectPersonWithName(
    DocumentManager* documentManager,
    const QString& nameLabel,
    const QString& initialName,
    const std::optional<PersonId>& initialId,
    QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, false, parent);
    dialog.setWindowTitle(tr("Select Person — %1").arg(nameLabel));
    dialog.m_nameLabel->setText(nameLabel + tr(":"));
    dialog.m_nameEdit->setText(initialName);
    if (initialId)
    {
        dialog.setPreselectedPersonIds({*initialId});
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    PersonSelectionResult result;
    result.name = dialog.name();

    // Single-select: get from current selection
    QList<PersonId> ids = dialog.selectedPersonIds();
    result.personIds = ids;
    return result;
}
```

**Step 4:** In `NeedsSubView.h`, replace `showAddNeedDialog()` and `showEditNeedDialog(...)` with a single method:

```cpp
// Remove:
void showAddNeedDialog();
void showEditNeedDialog(const PersonId& personId, const FamilyId& familyId);

// Add:
void showNeedDialog(const std::optional<PersonId>& personId);
```

**Step 5:** In `NeedsSubView.cpp`, implement `showNeedDialog`:

```cpp
void NeedsSubView::showNeedDialog(const std::optional<PersonId>& personId)
{
    QString initialName;
    std::optional<PersonId> initialPersonId = personId;

    if (personId)
    {
        const Document& doc = m_documentManager->document();
        std::optional<Person> personOpt = doc.findPersonById(*personId);
        if (personOpt)
        {
            initialName = personOpt->specialNeedNote();
        }
    }

    std::optional<PersonSelectionResult> result = WardListDialog::selectPersonWithName(
        m_documentManager, tr("Need"), initialName, initialPersonId, this);

    // Note: empty name is intentional for needs — it clears the special need note
    if (!result || result->personIds.isEmpty())
    {
        return;
    }

    PersonId selectedPersonId = result->personIds.first();
    const Document& doc = m_documentManager->document();
    std::optional<FamilyId> familyId = doc.familyIdForPerson(selectedPersonId);
    std::optional<Person> personOpt = doc.findPersonById(selectedPersonId);
    if (personOpt && familyId)
    {
        Person updatedPerson = *personOpt;
        updatedPerson.setSpecialNeedNote(result->name);
        updatePersonInFamily(m_documentManager, *familyId, updatedPerson);
    }
}
```

**Step 6:** Remove the old `showAddNeedDialog()` and `showEditNeedDialog(...)` methods entirely.

**Step 7:** Update `onContextMenu` in `NeedsSubView.cpp`. The current context menu uses inline lambda-connected actions. Replace them with named slot calls:

In the context menu's "Add..." action, replace the call to `showAddNeedDialog()` with `showNeedDialog(std::nullopt)`.

In the "Edit..." action, replace `showEditNeedDialog(personId, familyId)` with `showNeedDialog(personId)`. Since `showNeedDialog` only needs a `PersonId` (not a `FamilyId`), the lambda that captures both can be simplified. If the action currently uses a lambda to capture the personId, convert it to store `m_contextPersonId` and use a named slot, following the same pattern as TeamsView/EmergencyAssetView context menus.

Also update the "Delete" action's call to `deleteNeed(personId, familyId)` — this method's signature doesn't change, so just verify it still compiles.

**Step 8:** Build and verify the app compiles.

**Step 9:** Commit.

```
feat: unify NeedsSubView add/edit into WardListDialog with name field
```

---

### Task 6: Clean up unused code

**Files:**
- Modify: `src/widgets/WardListDialog.h`
- Modify: `src/widgets/WardListDialog.cpp`

**Step 1:** Check if the old `selectPerson` (without name) is still used anywhere. If NeedsSubView was the only caller and it now uses `selectPersonWithName`, remove the old `selectPerson` static method.

**Step 2:** Check if `selectFamily` (single-select without name) has any callers. If not, remove it.

**Step 3:** Remove `setPreselectedFamilyIds` and `setPreselectedPersonIds` if they are no longer called externally (the static methods handle preselection internally).

**Step 4:** Build and verify.

**Step 5:** Commit.

```
refactor: remove unused WardListDialog convenience methods
```
