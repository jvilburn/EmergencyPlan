#include "WardListView.h"
#include "ActionButtonsWidget.h"
#include "ContactAttemptDialog.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "EmergencyProgressBar.h"
#include "Family.h"
#include "FamilyTreeModel.h"
#include "Filter.h"
#include "FilterBar.h"
#include "NotifyDialog.h"
#include "SelectionPreservingTreeView.h"
#include "TaskDialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

namespace StatusFilterIndex
{
    constexpr int All = 0;
    constexpr int Remaining = 1;
    constexpr int NeedsHelp = 2;
    constexpr int OK = 3;
    constexpr int UnableToReach = 4;
}

WardListView::WardListView(DocumentManager* documentManager,
                           EmergencyManager* emergencyManager,
                           QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_emergencyManager(emergencyManager)
{
    setMinimumWidth(250);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // FilterBar owns Filter
    m_filterBar = new FilterBar(documentManager, this);
    layout->addWidget(m_filterBar);

    // Create model with filter from FilterBar
    m_model = new FamilyTreeModel(documentManager, emergencyManager, m_filterBar->filter(), false, this);

    // Emergency progress bar and filter tabs (initially hidden)
    setupEmergencyWidgets();
    layout->addWidget(m_emergencyPanel);

    // Tree view with selection preservation
    m_treeView = new SelectionPreservingTreeView(m_model, this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setAnimated(true);
    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->header()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_treeView, 1);

    // Connect tree events
    connect(m_treeView, &SelectionPreservingTreeView::selectionChanged,
            this, &WardListView::onSelectionChanged);
    connect(m_treeView, &QTreeView::expanded,
            this, &WardListView::onItemExpanded);
    connect(m_treeView, &QTreeView::collapsed,
            this, &WardListView::onItemCollapsed);

    // Context menu for task actions
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QWidget::customContextMenuRequested,
            this, &WardListView::onTreeContextMenu);

    // Connect model signals
    connect(m_model, &QAbstractItemModel::modelReset,
            this, &WardListView::onModelReset);
    connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, &WardListView::onRowsRemoved);
    connect(m_model, &QAbstractItemModel::rowsInserted,
            this, &WardListView::onRowsInserted);

    // Emergency lifecycle
    if (m_emergencyManager)
    {
        connect(m_emergencyManager, &EmergencyManager::emergencyStarted,
                this, &WardListView::onEmergencyStateChanged);
        connect(m_emergencyManager, &EmergencyManager::emergencyEnded,
                this, &WardListView::onEmergencyStateChanged);
        connect(m_emergencyManager, &EmergencyManager::responseDataChanged,
                this, &WardListView::updateFilterTabCounts);
    }

    // Emit initial visible families
    emit visibleFamiliesChanged(visibleFamilyIdsList());
}

std::optional<FamilyId> WardListView::selectedFamilyId() const
{
    QModelIndex current = m_treeView->currentIndex();
    if (!current.isValid())
    {
        return std::nullopt;
    }
    return m_model->familyIdAt(current);
}

void WardListView::setSelectedFamilyId(const std::optional<FamilyId>& id)
{
    if (!id)
    {
        clearSelection();
        return;
    }
    QModelIndex familyIndex = m_model->indexForFamilyId(*id);
    if (familyIndex.isValid())
    {
        m_treeView->setCurrentIndex(familyIndex);
        m_treeView->scrollTo(familyIndex);
    }
}

void WardListView::clearSelection()
{
    m_treeView->clearSelection();
}

void WardListView::selectFamily(const FamilyId& familyId)
{
    setSelectedFamilyId(familyId);
}

QList<FamilyId> WardListView::visibleFamilyIdsList() const
{
    return m_model->familyIds();
}

HighlightInfo WardListView::highlightInfo() const
{
    HighlightInfo info;
    auto selected = selectedFamilyId();
    if (selected)
    {
        info.highlightedFamilyIds.insert(*selected);
    }
    return info;
}

QSet<FamilyId> WardListView::visibleFamilyIds() const
{
    QList<FamilyId> list = visibleFamilyIdsList();
    return QSet<FamilyId>(list.begin(), list.end());
}

QString WardListView::familyStatusIcon(const FamilyId& familyId) const
{
    if (!m_emergencyManager || !m_emergencyManager->isActive())
    {
        return {};
    }

    EffectiveContactStatus status = m_emergencyManager->familyStatus(familyId);
    switch (status)
    {
        case EffectiveContactStatus::OK:
            return QString::fromUtf8("\xe2\x9c\x93");  // ✓
        case EffectiveContactStatus::NeedsHelp:
            return QString::fromUtf8("\xe2\x9a\x91");  // ⚑
        case EffectiveContactStatus::UnableToReach:
            return "?";
        case EffectiveContactStatus::NotContacted:
            return {};
    }
    return {};
}

void WardListView::onSelectionChanged()
{
    emit highlightChanged();

    // Expand the selected family if it's a family row
    QModelIndex current = m_treeView->currentIndex();
    if (current.isValid())
    {
        FamilyTreeModel::RowType type = m_model->rowTypeAt(current);
        if (type == FamilyTreeModel::RowType::Family && !m_treeView->isExpanded(current))
        {
            m_treeView->expand(current);
        }
    }
}

Filter* WardListView::filter() const
{
    return m_filterBar->filter();
}

void WardListView::onModelReset()
{
    // Clear all action widgets - they were deleted by the model reset
    m_actionWidgets.clear();
    emit visibleFamiliesChanged(visibleFamilyIdsList());
}

void WardListView::onRowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(first)
    Q_UNUSED(last)

    // When a family's children are removed (during surgical update),
    // the action widget is deleted by Qt. Clean up our tracking.
    if (parent.isValid())
    {
        FamilyTreeModel::RowType type = m_model->rowTypeAt(parent);
        if (type == FamilyTreeModel::RowType::Family)
        {
            auto familyId = m_model->familyIdAt(parent);
            if (familyId)
            {
                m_actionWidgets.remove(*familyId);
            }
        }
    }
}

void WardListView::onRowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(first)
    Q_UNUSED(last)

    // When children are inserted under an expanded family, re-attach action buttons
    if (parent.isValid())
    {
        FamilyTreeModel::RowType type = m_model->rowTypeAt(parent);
        if (type == FamilyTreeModel::RowType::Family && m_treeView->isExpanded(parent))
        {
            attachActionButtons(parent);
        }
    }
}

void WardListView::onItemExpanded(const QModelIndex& index)
{
    FamilyTreeModel::RowType type = m_model->rowTypeAt(index);
    if (type == FamilyTreeModel::RowType::Family)
    {
        attachActionButtons(index);
    }
}

void WardListView::onItemCollapsed(const QModelIndex& index)
{
    FamilyTreeModel::RowType type = m_model->rowTypeAt(index);
    if (type == FamilyTreeModel::RowType::Family)
    {
        auto familyId = m_model->familyIdAt(index);
        if (familyId)
        {
            detachActionButtons(*familyId);
        }
    }
}

void WardListView::attachActionButtons(const QModelIndex& familyIndex)
{
    auto familyId = m_model->familyIdAt(familyIndex);
    if (!familyId)
    {
        return;
    }

    // Already have buttons for this family?
    if (m_actionWidgets.contains(*familyId))
    {
        return;
    }

    // Find the Actions row (last child of family)
    int childCount = m_model->rowCount(familyIndex);
    for (int i = 0; i < childCount; ++i)
    {
        QModelIndex childIndex = m_model->index(i, 0, familyIndex);
        FamilyTreeModel::RowType childType = m_model->rowTypeAt(childIndex);

        if (childType == FamilyTreeModel::RowType::Actions)
        {
            // Look up phone number for emergency quick-call display
            QString phoneNumber;
            const auto& families = m_documentManager->document().families();
            auto it = families.constFind(*familyId);
            if (it != families.constEnd())
            {
                phoneNumber = it.value().displayPhone();
            }

            ActionButtonsWidget* widget = new ActionButtonsWidget(
                *familyId, m_emergencyManager, phoneNumber, m_treeView);

            connect(widget, &ActionButtonsWidget::editRequested,
                    this, &WardListView::editFamilyRequested);
            connect(widget, &ActionButtonsWidget::deleteRequested,
                    this, &WardListView::deleteFamilyRequested);
            connect(widget, &ActionButtonsWidget::logContactRequested,
                    this, &WardListView::onLogContactRequested);
            connect(widget, &ActionButtonsWidget::addTaskRequested,
                    this, &WardListView::onAddTaskRequested);

            m_treeView->setIndexWidget(childIndex, widget);
            m_actionWidgets[*familyId] = widget;
            break;
        }
    }
}

void WardListView::detachActionButtons(const FamilyId& familyId)
{
    if (!m_actionWidgets.contains(familyId))
    {
        return;
    }

    // The widget will be deleted by setIndexWidget(nullptr)
    QModelIndex familyIndex = m_model->indexForFamilyId(familyId);
    if (familyIndex.isValid())
    {
        int childCount = m_model->rowCount(familyIndex);
        for (int i = 0; i < childCount; ++i)
        {
            QModelIndex childIndex = m_model->index(i, 0, familyIndex);
            FamilyTreeModel::RowType childType = m_model->rowTypeAt(childIndex);

            if (childType == FamilyTreeModel::RowType::Actions)
            {
                m_treeView->setIndexWidget(childIndex, nullptr);
                break;
            }
        }
    }

    m_actionWidgets.remove(familyId);
}

void WardListView::setupEmergencyWidgets()
{
    m_emergencyPanel = new QWidget(this);
    QVBoxLayout* panelLayout = new QVBoxLayout(m_emergencyPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(2);

    // Progress bar
    m_progressBar = new EmergencyProgressBar(m_emergencyManager, m_emergencyPanel);
    panelLayout->addWidget(m_progressBar);

    // Filter tabs row
    QWidget* tabRow = new QWidget(m_emergencyPanel);
    QHBoxLayout* tabLayout = new QHBoxLayout(tabRow);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(2);

    // Tab definitions matching StatusFilterIndex constants
    QStringList tabLabels = {tr("All"), tr("Remaining"), tr("Needs Help"), tr("OK"), tr("Unable to Reach")};

    for (int i = 0; i < tabLabels.size(); ++i)
    {
        QToolButton* tab = new QToolButton(tabRow);
        tab->setText(tabLabels.at(i));
        tab->setCheckable(true);
        tab->setAutoExclusive(true);
        tab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        tab->setToolButtonStyle(Qt::ToolButtonTextOnly);
        tab->setProperty("statusIndex", i);
        tabLayout->addWidget(tab);
        m_filterTabs.append(tab);

        connect(tab, &QToolButton::clicked,
                this, &WardListView::onStatusFilterTabClicked);
    }

    // Default: "All" selected
    m_filterTabs.first()->setChecked(true);

    panelLayout->addWidget(tabRow);

    // Initially hidden
    m_emergencyPanel->hide();
}

void WardListView::onEmergencyStateChanged()
{
    bool active = m_emergencyManager && m_emergencyManager->isActive();
    m_emergencyPanel->setVisible(active);

    if (!active)
    {
        // Clear contact status filter when emergency ends
        m_filterBar->filter()->setContactStatusFilter(std::nullopt);
        if (!m_filterTabs.isEmpty())
        {
            m_filterTabs.first()->setChecked(true);
        }
    }
    else
    {
        updateFilterTabCounts();
    }
}

void WardListView::onStatusFilterTabClicked()
{
    QToolButton* tab = qobject_cast<QToolButton*>(sender());
    if (!tab)
    {
        return;
    }

    int statusIndex = tab->property("statusIndex").toInt();
    onStatusFilterClicked(statusIndex);
}

void WardListView::onStatusFilterClicked(int statusIndex)
{
    Filter* f = m_filterBar->filter();

    switch (statusIndex)
    {
        case StatusFilterIndex::All:
            f->setContactStatusFilter(std::nullopt);
            break;
        case StatusFilterIndex::Remaining:
            f->setContactStatusFilter(EffectiveContactStatus::NotContacted);
            break;
        case StatusFilterIndex::NeedsHelp:
            f->setContactStatusFilter(EffectiveContactStatus::NeedsHelp);
            break;
        case StatusFilterIndex::OK:
            f->setContactStatusFilter(EffectiveContactStatus::OK);
            break;
        case StatusFilterIndex::UnableToReach:
            f->setContactStatusFilter(EffectiveContactStatus::UnableToReach);
            break;
    }
}

void WardListView::updateFilterTabCounts()
{
    if (!m_emergencyManager || !m_emergencyManager->isActive())
    {
        return;
    }

    int total = m_emergencyManager->totalFamilies();
    int ok = m_emergencyManager->countByStatus(EffectiveContactStatus::OK);
    int needsHelp = m_emergencyManager->countByStatus(EffectiveContactStatus::NeedsHelp);
    int unable = m_emergencyManager->countByStatus(EffectiveContactStatus::UnableToReach);
    int remaining = m_emergencyManager->countByStatus(EffectiveContactStatus::NotContacted);

    m_filterTabs[StatusFilterIndex::All]->setText(tr("All (%1)").arg(total));
    m_filterTabs[StatusFilterIndex::Remaining]->setText(tr("Remaining (%1)").arg(remaining));
    m_filterTabs[StatusFilterIndex::NeedsHelp]->setText(tr("Needs Help (%1)").arg(needsHelp));
    m_filterTabs[StatusFilterIndex::OK]->setText(tr("OK (%1)").arg(ok));
    m_filterTabs[StatusFilterIndex::UnableToReach]->setText(tr("Unable to Reach (%1)").arg(unable));
}

void WardListView::onLogContactRequested(const FamilyId& familyId)
{
    ContactAttemptDialog dialog(m_documentManager, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    std::optional<ContactAttempt> attempt = dialog.result();
    if (attempt)
    {
        m_emergencyManager->addContactAttempt(familyId, *attempt);
    }
}

void WardListView::onAddTaskRequested(const FamilyId& familyId)
{
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

void WardListView::onEditTaskRequested(const FamilyId& familyId, const TaskId& taskId)
{
    const FamilyResponseRecord* record = m_emergencyManager->recordForFamily(familyId);
    if (!record)
    {
        return;
    }

    // Find the existing task
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

void WardListView::onNotifyTaskRequested(const FamilyId& familyId, const TaskId& taskId)
{
    NotifyDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    std::optional<TaskNotification> notification = dialog.result();
    if (notification)
    {
        m_emergencyManager->notifyAssignee(familyId, taskId, *notification);
    }
}

void WardListView::onResolveTaskRequested(const FamilyId& familyId, const TaskId& taskId)
{
    bool ok = false;
    QString notes = QInputDialog::getText(
        this, tr("Resolve Task"), tr("Resolution notes (optional):"),
        QLineEdit::Normal, QString(), &ok);

    if (!ok)
    {
        return;
    }

    m_emergencyManager->resolveTask(familyId, taskId, notes.trimmed());
}

void WardListView::onTreeContextMenu(const QPoint& pos)
{
    QModelIndex index = m_treeView->indexAt(pos);
    if (!index.isValid())
    {
        return;
    }

    FamilyTreeModel::RowType rowType = m_model->rowTypeAt(index);
    if (rowType != FamilyTreeModel::RowType::Task)
    {
        return;
    }

    // Get family and task IDs
    std::optional<FamilyId> familyId = m_model->familyIdAt(index);
    QString taskIdStr = index.data(FamilyTreeModel::TaskIdRole).toString();
    if (!familyId || taskIdStr.isEmpty())
    {
        return;
    }

    TaskId taskId = TaskId::fromString(taskIdStr);

    // Find the task to determine available actions
    const FamilyResponseRecord* record = m_emergencyManager->recordForFamily(*familyId);
    if (!record)
    {
        return;
    }

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
        return;
    }

    QMenu menu(this);

    QAction* editAction = menu.addAction(tr("Edit Task"));
    QAction* notifyAction = nullptr;
    QAction* resolveAction = nullptr;

    if (task->isAssigned() && !task->isNotified())
    {
        notifyAction = menu.addAction(tr("Notify Assignee"));
    }

    if (!task->isResolved())
    {
        resolveAction = menu.addAction(tr("Resolve"));
    }

    menu.addSeparator();
    QAction* deleteAction = menu.addAction(tr("Delete Task"));

    QAction* chosen = menu.exec(m_treeView->viewport()->mapToGlobal(pos));
    if (!chosen)
    {
        return;
    }

    if (chosen == editAction)
    {
        onEditTaskRequested(*familyId, taskId);
    }
    else if (chosen == notifyAction)
    {
        onNotifyTaskRequested(*familyId, taskId);
    }
    else if (chosen == resolveAction)
    {
        onResolveTaskRequested(*familyId, taskId);
    }
    else if (chosen == deleteAction)
    {
        m_emergencyManager->removeTask(*familyId, taskId);
    }
}
