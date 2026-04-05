# Consolidated Task List View — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** Add a "Tasks" sidebar tab showing all emergency response tasks across all families in a single consolidated table.

**Architecture:** New `TaskListModel` (QAbstractTableModel) aggregates tasks from all `FamilyResponseRecord`s. New `TaskListView` (QWidget) wraps it with a button bar (Add Task, Edit, Delete) and a QTreeView with a checkable resolved column. `SidebarWidget` gets show/hide page support. `MainWindow` adds/removes the Tasks tab on emergency start/end.

**Tech Stack:** Qt 6 (QAbstractTableModel, QSortFilterProxyModel, QTreeView, QWidget)

---

### Task 1: Add show/hide page support to SidebarWidget

SidebarWidget currently only has `addPage`. We need `setPageVisible(int index, bool visible)` to show/hide the Tasks tab dynamically.

**Files:**
- Modify: `src/widgets/SidebarWidget.h`
- Modify: `src/widgets/SidebarWidget.cpp`

**Step 1: Add `setPageVisible` declaration**

In `SidebarWidget.h`, add after `setCurrentIndex`:

```cpp
void setPageVisible(int index, bool visible);
```

**Step 2: Implement `setPageVisible`**

In `SidebarWidget.cpp`, add:

```cpp
void SidebarWidget::setPageVisible(int index, bool visible)
{
    QAbstractButton* button = m_buttonGroup->button(index);
    if (!button)
    {
        return;
    }

    button->setVisible(visible);

    // If hiding the currently selected page, switch to first visible page
    if (!visible && m_stack->currentIndex() == index)
    {
        for (int i = 0; i < m_stack->count(); ++i)
        {
            QAbstractButton* other = m_buttonGroup->button(i);
            if (other && other->isVisible())
            {
                setCurrentIndex(i);
                return;
            }
        }
    }
}
```

**Step 3: Build and verify**

Run `build.bat`. Expected: compiles successfully, no functional change.

**Step 4: Commit**

```
feat: add setPageVisible to SidebarWidget
```

---

### Task 2: Add `reopenTask` to EmergencyManager

`ResponseTask::unresolve()` exists but `EmergencyManager` has no public method to reopen a resolved task. The checkbox toggle needs it.

**Files:**
- Modify: `src/services/EmergencyManager.h`
- Modify: `src/services/EmergencyManager.cpp`

**Step 1: Add declaration**

In `EmergencyManager.h`, after `resolveTask`:

```cpp
void reopenTask(const FamilyId& familyId, const TaskId& taskId);
```

**Step 2: Implement**

In `EmergencyManager.cpp`, after `resolveTask` method. Follow the same pattern as `resolveTask`:

```cpp
void EmergencyManager::reopenTask(const FamilyId& familyId, const TaskId& taskId)
{
    if (!m_response)
    {
        return;
    }

    FamilyResponseRecord* record = m_response->mutableRecord(familyId);
    if (!record)
    {
        return;
    }

    ResponseTask* task = record->mutableTask(taskId);
    if (!task)
    {
        return;
    }
    task->reopen();
    persistResponseData();
    emit responseDataChanged();
}
```

**Step 3: Build and verify**

Run `build.bat`. Expected: compiles.

**Step 4: Commit**

```
feat: add reopenTask to EmergencyManager
```

---

### Task 3: Create TaskListModel

A flat table model that collects all tasks from all families.

**Files:**
- Create: `src/listmodels/TaskListModel.h`
- Create: `src/listmodels/TaskListModel.cpp`
- Modify: `CMakeLists.txt` (add both files)

**Step 1: Create TaskListModel.h**

```cpp
#pragma once

#include "Id.h"

#include <QAbstractTableModel>
#include <optional>

class DocumentManager;
class EmergencyManager;

class TaskListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        ResolvedCol = 0,
        AssignedToCol,
        CategoryCol,
        FamilyCol,
        DescriptionCol,
        ColumnCount
    };

    explicit TaskListModel(DocumentManager* documentManager,
                           EmergencyManager* emergencyManager,
                           QObject* parent);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    FamilyId familyIdForRow(int row) const;
    TaskId taskIdForRow(int row) const;

    void rebuild();

private:
    struct TaskEntry
    {
        FamilyId familyId;
        QString familyName;
        TaskId taskId;
        QString category;
        QString description;
        QString assignedTo;
        bool resolved = false;
    };

    QString resolveAssignedName(const std::optional<TeamId>& teamId,
                                const std::optional<PersonId>& personId) const;

    DocumentManager* m_documentManager;
    EmergencyManager* m_emergencyManager;
    QList<TaskEntry> m_entries;
};
```

**Step 2: Create TaskListModel.cpp**

```cpp
#include "TaskListModel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "Person.h"
#include "Team.h"

TaskListModel::TaskListModel(DocumentManager* documentManager,
                             EmergencyManager* emergencyManager,
                             QObject* parent)
    : QAbstractTableModel(parent)
    , m_documentManager(documentManager)
    , m_emergencyManager(emergencyManager)
{
    connect(m_emergencyManager, &EmergencyManager::responseDataChanged,
            this, &TaskListModel::rebuild);
}

int TaskListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_entries.size();
}

int TaskListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return ColumnCount;
}

QVariant TaskListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
    {
        return QVariant();
    }

    const TaskEntry& entry = m_entries.at(index.row());

    if (role == Qt::CheckStateRole && index.column() == ResolvedCol)
    {
        return entry.resolved ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case FamilyCol:
            return entry.familyName;
        case CategoryCol:
            return entry.category;
        case DescriptionCol:
            return entry.description;
        case AssignedToCol:
            return entry.assignedTo;
        default:
            return QVariant();
        }
    }

    return QVariant();
}

QVariant TaskListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return QVariant();
    }

    switch (section)
    {
    case ResolvedCol:
        return QString();
    case FamilyCol:
        return tr("Family");
    case CategoryCol:
        return tr("Category");
    case DescriptionCol:
        return tr("Description");
    case AssignedToCol:
        return tr("Assigned To");
    default:
        return QVariant();
    }
}

Qt::ItemFlags TaskListModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.column() == ResolvedCol)
    {
        f |= Qt::ItemIsUserCheckable;
    }
    return f;
}

bool TaskListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::CheckStateRole || index.column() != ResolvedCol)
    {
        return false;
    }

    const TaskEntry& entry = m_entries.at(index.row());
    bool checked = (value.toInt() == Qt::Checked);

    if (checked)
    {
        m_emergencyManager->resolveTask(entry.familyId, entry.taskId, QString());
    }
    else
    {
        m_emergencyManager->reopenTask(entry.familyId, entry.taskId);
    }

    // rebuild() will be triggered by responseDataChanged signal
    return true;
}

FamilyId TaskListModel::familyIdForRow(int row) const
{
    return m_entries.at(row).familyId;
}

TaskId TaskListModel::taskIdForRow(int row) const
{
    return m_entries.at(row).taskId;
}

void TaskListModel::rebuild()
{
    beginResetModel();
    m_entries.clear();

    if (!m_emergencyManager->isActive())
    {
        endResetModel();
        return;
    }

    const EmergencyResponse& response = m_emergencyManager->response();
    const QHash<FamilyId, FamilyResponseRecord>& records = response.familyRecords();
    const QHash<FamilyId, Family>& families = m_documentManager->document().families();

    for (auto it = records.constBegin(); it != records.constEnd(); ++it)
    {
        const FamilyId& familyId = it.key();
        const FamilyResponseRecord& record = it.value();

        if (record.tasks().isEmpty())
        {
            continue;
        }

        QString familyName;
        auto famIt = families.constFind(familyId);
        if (famIt != families.constEnd())
        {
            familyName = famIt.value().headOfHousehold();
        }

        for (const ResponseTask& task : record.tasks())
        {
            TaskEntry entry;
            entry.familyId = familyId;
            entry.familyName = familyName;
            entry.taskId = task.id();
            entry.category = task.category();
            entry.description = task.description();
            entry.assignedTo = resolveAssignedName(task.assignedTeamId(), task.assignedPersonId());
            entry.resolved = task.isResolved();
            m_entries.append(entry);
        }
    }

    // Sort: unresolved first, then assignedTo, then category, then family
    std::sort(m_entries.begin(), m_entries.end(),
              [](const TaskEntry& a, const TaskEntry& b)
              {
                  if (a.resolved != b.resolved)
                  {
                      return !a.resolved;  // unresolved first
                  }
                  // Assigned before unassigned, then alphabetical
                  bool aAssigned = !a.assignedTo.isEmpty();
                  bool bAssigned = !b.assignedTo.isEmpty();
                  if (aAssigned != bAssigned)
                  {
                      return aAssigned;
                  }
                  if (aAssigned && bAssigned)
                  {
                      int cmp = a.assignedTo.compare(b.assignedTo, Qt::CaseInsensitive);
                      if (cmp != 0)
                      {
                          return cmp < 0;
                      }
                  }
                  int catCmp = a.category.compare(b.category, Qt::CaseInsensitive);
                  if (catCmp != 0)
                  {
                      return catCmp < 0;
                  }
                  return a.familyName.compare(b.familyName, Qt::CaseInsensitive) < 0;
              });

    endResetModel();
}

QString TaskListModel::resolveAssignedName(const std::optional<TeamId>& teamId,
                                           const std::optional<PersonId>& personId) const
{
    if (teamId)
    {
        auto it = m_documentManager->document().teams().constFind(*teamId);
        if (it != m_documentManager->document().teams().constEnd())
        {
            return it.value().name();
        }
    }
    if (personId)
    {
        const QHash<FamilyId, Family>& families = m_documentManager->document().families();
        for (auto it = families.constBegin(); it != families.constEnd(); ++it)
        {
            for (const Person& person : it.value().members())
            {
                if (person.id() == *personId)
                {
                    return person.displayName();
                }
            }
        }
    }
    return QString();
}
```

**Step 3: Add to CMakeLists.txt**

Add `src/listmodels/TaskListModel.cpp` to the SOURCES list (near other listmodel .cpp files) and `src/listmodels/TaskListModel.h` to the HEADERS list (near other listmodel .h files).

**Step 4: Build and verify**

Run `build.bat`. Expected: compiles (model is unused so far).

**Step 5: Commit**

```
feat: add TaskListModel for consolidated task list
```

---

### Task 4: Create TaskListView

The view widget with button bar and table.

**Files:**
- Create: `src/widgets/TaskListView.h`
- Create: `src/widgets/TaskListView.cpp`
- Modify: `CMakeLists.txt` (add both files)

**Step 1: Create TaskListView.h**

```cpp
#pragma once

#include <QWidget>

class QTreeView;
class QPushButton;
class DocumentManager;
class EmergencyManager;
class TaskListModel;

class TaskListView : public QWidget
{
    Q_OBJECT

public:
    explicit TaskListView(DocumentManager* documentManager,
                          EmergencyManager* emergencyManager,
                          QWidget* parent);

    void rebuild();

private slots:
    void onAddTask();
    void onEditTask();
    void onDeleteTask();
    void onSelectionChanged();

private:
    DocumentManager* m_documentManager;
    EmergencyManager* m_emergencyManager;
    TaskListModel* m_model;
    QTreeView* m_treeView;
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;
};
```

**Step 2: Create TaskListView.cpp**

```cpp
#include "TaskListView.h"
#include "TaskListModel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "TaskDialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QInputDialog>

TaskListView::TaskListView(DocumentManager* documentManager,
                           EmergencyManager* emergencyManager,
                           QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_emergencyManager(emergencyManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Button bar
    QHBoxLayout* buttonBar = new QHBoxLayout();
    buttonBar->setContentsMargins(4, 4, 4, 0);

    m_addButton = new QPushButton(tr("Add Task"), this);
    m_editButton = new QPushButton(tr("Edit"), this);
    m_deleteButton = new QPushButton(tr("Delete"), this);
    m_deleteButton->setStyleSheet("color: #c0392b;");

    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);

    buttonBar->addWidget(m_addButton);
    buttonBar->addWidget(m_editButton);
    buttonBar->addWidget(m_deleteButton);
    buttonBar->addStretch();

    layout->addLayout(buttonBar);

    // Table
    m_model = new TaskListModel(documentManager, emergencyManager, this);

    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_model);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSortingEnabled(false);  // sorting is built into the model

    // Column sizing
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->header()->setSectionResizeMode(TaskListModel::ResolvedCol, QHeaderView::ResizeToContents);
    m_treeView->header()->setSectionResizeMode(TaskListModel::AssignedToCol, QHeaderView::ResizeToContents);
    m_treeView->header()->setSectionResizeMode(TaskListModel::CategoryCol, QHeaderView::ResizeToContents);
    m_treeView->header()->setSectionResizeMode(TaskListModel::FamilyCol, QHeaderView::ResizeToContents);
    m_treeView->header()->setSectionResizeMode(TaskListModel::DescriptionCol, QHeaderView::Stretch);

    layout->addWidget(m_treeView);

    // Connections
    connect(m_addButton, &QPushButton::clicked, this, &TaskListView::onAddTask);
    connect(m_editButton, &QPushButton::clicked, this, &TaskListView::onEditTask);
    connect(m_deleteButton, &QPushButton::clicked, this, &TaskListView::onDeleteTask);
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &TaskListView::onSelectionChanged);
}

void TaskListView::rebuild()
{
    m_model->rebuild();
}

void TaskListView::onAddTask()
{
    // Ask which family the task is for
    const QHash<FamilyId, Family>& families = m_documentManager->document().families();
    QList<QPair<QString, FamilyId>> familyList;
    for (auto it = families.constBegin(); it != families.constEnd(); ++it)
    {
        familyList.append({it.value().headOfHousehold(), it.key()});
    }
    std::sort(familyList.begin(), familyList.end(),
              [](const auto& a, const auto& b) { return a.first.toLower() < b.first.toLower(); });

    QStringList names;
    for (const auto& entry : familyList)
    {
        names.append(entry.first);
    }

    bool ok = false;
    QString selected = QInputDialog::getItem(
        this, tr("Add Task"), tr("Family:"), names, 0, false, &ok);
    if (!ok || selected.isEmpty())
    {
        return;
    }

    int selectedIndex = names.indexOf(selected);
    if (selectedIndex < 0)
    {
        return;
    }

    FamilyId familyId = familyList.at(selectedIndex).second;

    TaskDialog dialog(m_documentManager, m_emergencyManager, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    std::optional<ResponseTask> task = dialog.result();
    if (task)
    {
        m_emergencyManager->addTask(familyId, *task);
    }
}

void TaskListView::onEditTask()
{
    QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid())
    {
        return;
    }

    FamilyId familyId = m_model->familyIdForRow(index.row());
    TaskId taskId = m_model->taskIdForRow(index.row());

    const FamilyResponseRecord* record = m_emergencyManager->recordForFamily(familyId);
    if (!record)
    {
        return;
    }

    const ResponseTask* existingTask = nullptr;
    for (const ResponseTask& t : record->tasks())
    {
        if (t.id() == taskId)
        {
            existingTask = &t;
            break;
        }
    }
    if (!existingTask)
    {
        return;
    }

    TaskDialog dialog(m_documentManager, m_emergencyManager, this);
    dialog.setTask(*existingTask);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    std::optional<ResponseTask> updatedTask = dialog.result();
    if (updatedTask)
    {
        m_emergencyManager->updateTask(familyId, *updatedTask);
    }
}

void TaskListView::onDeleteTask()
{
    QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid())
    {
        return;
    }

    FamilyId familyId = m_model->familyIdForRow(index.row());
    TaskId taskId = m_model->taskIdForRow(index.row());

    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("Delete Task"), tr("Delete this task?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes)
    {
        m_emergencyManager->removeTask(familyId, taskId);
    }
}

void TaskListView::onSelectionChanged()
{
    bool hasSelection = m_treeView->currentIndex().isValid();
    m_editButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
}
```

**Step 3: Add to CMakeLists.txt**

Add `src/widgets/TaskListView.cpp` to SOURCES and `src/widgets/TaskListView.h` to HEADERS.

**Step 4: Build and verify**

Run `build.bat`. Expected: compiles (widget not yet used).

**Step 5: Commit**

```
feat: add TaskListView widget for consolidated task list
```

---

### Task 5: Wire TaskListView into MainWindow

Add the Tasks tab to the sidebar, initially hidden. Show it when an emergency starts, hide when it ends.

**Files:**
- Modify: `src/widgets/MainWindow.h`
- Modify: `src/widgets/MainWindow.cpp`

**Step 1: Add member and forward declaration**

In `MainWindow.h`, add forward declaration:

```cpp
class TaskListView;
```

Add member variable alongside the other view members:

```cpp
TaskListView* m_taskListView;
int m_taskListTabIndex = -1;
```

**Step 2: Create and add the Tasks tab in `setupUi`**

In `MainWindow.cpp`, include `TaskListView.h`. After the `m_needsView` line (end of row 1), add:

```cpp
m_taskListView = new TaskListView(m_documentManager, m_emergencyManager, this);
m_taskListTabIndex = m_sidebarTabs->count();
m_sidebarTabs->addPage(m_taskListView, tr("Tasks"));
m_sidebarTabs->setPageVisible(m_taskListTabIndex, false);
```

Update the `SidebarWidget` constructor's `firstRowCount` from 4 to 5 to keep Tasks on row 1.

**Step 3: Show/hide on emergency lifecycle**

In `onEmergencyStarted()`, add:

```cpp
m_taskListView->rebuild();
m_sidebarTabs->setPageVisible(m_taskListTabIndex, true);
```

In `onEmergencyEnded()`, add:

```cpp
m_sidebarTabs->setPageVisible(m_taskListTabIndex, false);
```

In `onArchiveViewOpened()`, add:

```cpp
m_taskListView->rebuild();
m_sidebarTabs->setPageVisible(m_taskListTabIndex, true);
```

In `onArchiveViewClosed()`, add after the existing if/else:

```cpp
m_sidebarTabs->setPageVisible(m_taskListTabIndex, m_emergencyManager->isActive());
if (m_emergencyManager->isActive())
{
    m_taskListView->rebuild();
}
```

**Step 4: Build and test**

Run `build.bat`. Expected: compiles. When running, the Tasks tab should appear only during active emergencies. Adding tasks from the Families view should show them in the Tasks tab.

**Step 5: Commit**

```
feat: wire Tasks tab into MainWindow with emergency lifecycle
```
