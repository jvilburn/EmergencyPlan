#include "TaskListView.h"
#include "TaskListModel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyActions.h"
#include "EmergencyResponse.h"
#include "Family.h"
#include "Team.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

TaskListView::TaskListView(QWidget* parent)
    : QWidget(parent)
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
    m_notifyButton = new QPushButton(tr("Notify"), this);

    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
    m_notifyButton->setEnabled(false);

    buttonBar->addWidget(m_addButton);
    buttonBar->addWidget(m_editButton);
    buttonBar->addWidget(m_deleteButton);
    buttonBar->addWidget(m_notifyButton);
    buttonBar->addStretch();

    layout->addLayout(buttonBar);

    // Table
    m_model = new TaskListModel(this);

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
    connect(m_notifyButton, &QPushButton::clicked, this, &TaskListView::onNotifyTask);
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
    const QHash<FamilyId, Family>& families = DocumentManager::instance()->document().families();
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

    std::optional<ResponseTask> task = EmergencyActions::addTask(familyId, this);
    if (task)
    {
        promptNotifyIfNeeded(*task);
    }
}

void TaskListView::onEditTask()
{
    QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid())
    {
        return;
    }

    std::optional<ResponseTask> saved = EmergencyActions::editTask(m_model->taskIdForRow(index.row()), this);
    if (saved)
    {
        promptNotifyIfNeeded(*saved);
    }
}

void TaskListView::onDeleteTask()
{
    QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid())
    {
        return;
    }

    // The task as the user sees it, which the removal takes as its start condition
    std::optional<ResponseTask> seen = EmergencyActions::findTask(m_model->taskIdForRow(index.row()));
    if (!seen)
    {
        return;
    }

    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("Delete Task"), tr("Delete this task?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes)
    {
        EmergencyActions::deleteTask(*seen);
    }
}

void TaskListView::onNotifyTask()
{
    QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid())
    {
        return;
    }

    EmergencyActions::notifyAssignee(m_model->taskIdForRow(index.row()), this);
}

void TaskListView::promptNotifyIfNeeded(const ResponseTask& task)
{
    if (task.isAssigned() && !task.isNotified())
    {
        EmergencyActions::notifyAssignee(task.id(), this);
    }
}

void TaskListView::onSelectionChanged()
{
    bool hasSelection = m_treeView->currentIndex().isValid();
    m_editButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);

    bool canNotify = false;
    if (hasSelection)
    {
        int row = m_treeView->currentIndex().row();
        canNotify = m_model->isAssignedForRow(row) && !m_model->isNotifiedForRow(row);
    }
    m_notifyButton->setEnabled(canNotify);

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

    std::optional<ResponseTask> task = EmergencyActions::findTask(m_model->taskIdForRow(row));
    if (!task || !task->assignment())
    {
        return info;
    }

    const Document& doc = DocumentManager::instance()->document();
    const TaskAssignment& assignment = *task->assignment();

    if (assignment.personId())
    {
        std::optional<FamilyId> assigneeFamilyId = doc.familyIdForPerson(*assignment.personId());
        if (assigneeFamilyId)
        {
            info.contactPointFamilyIds.insert(*assigneeFamilyId);
        }
    }
    else if (assignment.teamId())
    {
        std::optional<Team> team = doc.findTeamById(*assignment.teamId());
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
