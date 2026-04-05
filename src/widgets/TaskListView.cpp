#include "TaskListView.h"
#include "TaskListModel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "TaskDialog.h"
#include "Team.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

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
    connect(m_model, &QAbstractItemModel::modelReset,
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
        familyList.append(QPair<QString, FamilyId>(it.value().displayName(), it.key()));
    }
    std::sort(familyList.begin(), familyList.end(),
              [](const QPair<QString, FamilyId>& a, const QPair<QString, FamilyId>& b) { return a.first.toLower() < b.first.toLower(); });

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
    emit highlightChanged();
}

HighlightInfo TaskListView::highlightInfo() const
{
    HighlightInfo info;
    QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid())
    {
        return info;
    }

    int row = index.row();
    FamilyId familyId = m_model->familyIdForRow(row);
    info.highlightedFamilyIds.insert(familyId);

    const FamilyResponseRecord* record = m_emergencyManager->recordForFamily(familyId);
    if (!record)
    {
        return info;
    }

    TaskId taskId = m_model->taskIdForRow(row);
    const ResponseTask* task = nullptr;
    for (const ResponseTask& t : record->tasks())
    {
        if (t.id() == taskId)
        {
            task = &t;
            break;
        }
    }
    if (!task)
    {
        return info;
    }

    const Document& doc = m_documentManager->document();

    if (task->assignedPersonId())
    {
        std::optional<FamilyId> assigneeFamilyId = doc.familyIdForPerson(*task->assignedPersonId());
        if (assigneeFamilyId)
        {
            info.contactPointFamilyIds.insert(*assigneeFamilyId);
        }
    }
    else if (task->assignedTeamId())
    {
        std::optional<Team> team = doc.findTeamById(*task->assignedTeamId());
        if (team)
        {
            for (const PersonId& personId : team->memberIds())
            {
                std::optional<FamilyId> memberFamilyId = doc.familyIdForPerson(personId);
                if (memberFamilyId)
                {
                    info.contactPointFamilyIds.insert(*memberFamilyId);
                }
            }
        }
    }

    return info;
}

QSet<FamilyId> TaskListView::visibleFamilyIds() const
{
    return {};  // show all families
}

void TaskListView::clearSelection()
{
    m_treeView->clearSelection();
}

void TaskListView::selectFamily(const FamilyId& familyId)
{
    // Find the first task row for this family and select it
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        if (m_model->familyIdForRow(row) == familyId)
        {
            m_treeView->setCurrentIndex(m_model->index(row, 0));
            return;
        }
    }
}
