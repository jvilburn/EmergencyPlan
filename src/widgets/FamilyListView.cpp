#include "FamilyListView.h"
#include "ActionButtonsWidget.h"
#include "EmergencyActions.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "EmergencyProgressBar.h"
#include "Family.h"
#include "FamilyTreeModel.h"
#include "Filter.h"
#include "FilterBar.h"
#include "MarkerRenderer.h"
#include "SelectionPreservingTreeView.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace StatusFilterIndex
{
    constexpr int All = 0;
    constexpr int Remaining = 1;
    constexpr int OK = 2;
    constexpr int NeedsHelp = 3;
    constexpr int UnableToReach = 4;
}

FamilyListView::FamilyListView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(250);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // FilterBar owns Filter
    m_filterBar = new FilterBar(this);
    layout->addWidget(m_filterBar);

    // Create model with filter from FilterBar
    m_model = new FamilyTreeModel(m_filterBar->filter(), false, this);

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
            this, &FamilyListView::onSelectionChanged);
    connect(m_treeView, &QTreeView::expanded,
            this, &FamilyListView::onItemExpanded);
    connect(m_treeView, &QTreeView::collapsed,
            this, &FamilyListView::onItemCollapsed);

    // Context menu for task actions
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QWidget::customContextMenuRequested,
            this, &FamilyListView::onTreeContextMenu);

    // Connect model signals
    connect(m_model, &QAbstractItemModel::modelReset,
            this, &FamilyListView::onModelReset);
    connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, &FamilyListView::onRowsRemoved);
    connect(m_model, &QAbstractItemModel::rowsInserted,
            this, &FamilyListView::onRowsInserted);

    // Emergency lifecycle
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyStarted,
            this, &FamilyListView::onEmergencyStateChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyEnded,
            this, &FamilyListView::onEmergencyStateChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::responseDataChanged,
            this, &FamilyListView::updateFilterTabCounts);

    // Emit initial visible families
    emit visibleFamiliesChanged(visibleFamilyIdsList());
}

std::optional<FamilyId> FamilyListView::selectedFamilyId() const
{
    QModelIndex current = m_treeView->currentIndex();
    if (!current.isValid())
    {
        return std::nullopt;
    }
    return m_model->familyIdAt(current);
}

void FamilyListView::setSelectedFamilyId(const std::optional<FamilyId>& id)
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

void FamilyListView::clearSelection()
{
    m_treeView->clearSelection();
}

void FamilyListView::selectFamily(const FamilyId& familyId)
{
    setSelectedFamilyId(familyId);
}

QList<FamilyId> FamilyListView::visibleFamilyIdsList() const
{
    return m_model->familyIds();
}

HighlightInfo FamilyListView::highlightInfo() const
{
    HighlightInfo info;
    std::optional<FamilyId> selected = selectedFamilyId();
    if (selected)
    {
        info.highlightedFamilyIds.insert(*selected);
    }
    return info;
}

QSet<FamilyId> FamilyListView::visibleFamilyIds() const
{
    QList<FamilyId> list = visibleFamilyIdsList();
    return QSet<FamilyId>(list.begin(), list.end());
}

QString FamilyListView::familyStatusIcon(const FamilyId& familyId) const
{
    EffectiveContactStatus status = EmergencyManager::instance()->familyStatus(familyId);
    switch (status)
    {
        case EffectiveContactStatus::OK:
            return MarkerRenderer::STATUS_OK;
        case EffectiveContactStatus::NeedsHelp:
            return MarkerRenderer::STATUS_NEEDS_HELP;
        case EffectiveContactStatus::UnableToReach:
            return MarkerRenderer::STATUS_UNABLE_TO_REACH;
        case EffectiveContactStatus::NotContacted:
            return {};
    }
    return {};
}

void FamilyListView::onSelectionChanged()
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

Filter* FamilyListView::filter() const
{
    return m_filterBar->filter();
}

void FamilyListView::onModelReset()
{
    emit visibleFamiliesChanged(visibleFamilyIdsList());
}

void FamilyListView::onRowsRemoved(const QModelIndex& parent)
{
    // A family left the list. A family's own rows taking its Actions row take its buttons
    // with them.
    if (!parent.isValid())
    {
        emit visibleFamiliesChanged(visibleFamilyIdsList());
    }
}

void FamilyListView::onRowsInserted(const QModelIndex& parent)
{
    if (!parent.isValid())
    {
        emit visibleFamiliesChanged(visibleFamilyIdsList());
        return;
    }

    // Rows inserted under an expanded family may bring a new Actions row
    if (m_model->rowTypeAt(parent) == FamilyTreeModel::RowType::Family && m_treeView->isExpanded(parent))
    {
        attachActionButtons(parent);
    }
}

void FamilyListView::onItemExpanded(const QModelIndex& index)
{
    FamilyTreeModel::RowType type = m_model->rowTypeAt(index);
    if (type == FamilyTreeModel::RowType::Family)
    {
        attachActionButtons(index);
    }
}

void FamilyListView::onItemCollapsed(const QModelIndex& index)
{
    if (m_model->rowTypeAt(index) == FamilyTreeModel::RowType::Family)
    {
        detachActionButtons(index);
    }
}

QModelIndex FamilyListView::actionsIndex(const QModelIndex& familyIndex) const
{
    // The Actions row is the family's last child
    return m_model->index(m_model->rowCount(familyIndex) - 1, 0, familyIndex);
}

void FamilyListView::attachActionButtons(const QModelIndex& familyIndex)
{
    std::optional<FamilyId> familyId = m_model->familyIdAt(familyIndex);
    QModelIndex actions = actionsIndex(familyIndex);
    if (!familyId || !actions.isValid() || m_treeView->indexWidget(actions))
    {
        return;
    }

    ActionButtonsWidget* widget = new ActionButtonsWidget(*familyId, m_treeView);
    connect(widget, &ActionButtonsWidget::editRequested,
            this, &FamilyListView::editFamilyRequested);
    connect(widget, &ActionButtonsWidget::deleteRequested,
            this, &FamilyListView::deleteFamilyRequested);
    m_treeView->setIndexWidget(actions, widget);
}

void FamilyListView::detachActionButtons(const QModelIndex& familyIndex)
{
    // The view deletes the widget it held
    QModelIndex actions = actionsIndex(familyIndex);
    if (actions.isValid())
    {
        m_treeView->setIndexWidget(actions, nullptr);
    }
}

void FamilyListView::setupEmergencyWidgets()
{
    m_emergencyPanel = new QWidget(this);
    QVBoxLayout* panelLayout = new QVBoxLayout(m_emergencyPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(2);

    // Progress bar
    m_progressBar = new EmergencyProgressBar(m_emergencyPanel);
    panelLayout->addWidget(m_progressBar);

    // Filter tabs
    // Tab definitions matching StatusFilterIndex constants
    QStringList tabLabels = {tr("All"), tr("Remaining"), tr("OK"), tr("Needs Help"), tr("Unable to Reach")};

    QWidget* tabRow = new QWidget(m_emergencyPanel);
    QHBoxLayout* tabLayout = new QHBoxLayout(tabRow);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(2);

    for (int i = 0; i < tabLabels.size(); ++i)
    {
        QPushButton* tab = new QPushButton(m_emergencyPanel);
        tab->setText(tabLabels.at(i));
        tab->setCheckable(true);
        tab->setAutoExclusive(true);
        tab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        tab->setProperty("statusIndex", i);
        tabLayout->addWidget(tab);
        m_filterTabs.append(tab);

        connect(tab, &QPushButton::clicked,
                this, &FamilyListView::onStatusFilterTabClicked);
    }

    // Default: "All" selected
    m_filterTabs.first()->setChecked(true);

    panelLayout->addWidget(tabRow);

    // Initially hidden
    m_emergencyPanel->hide();
}

void FamilyListView::onEmergencyStateChanged()
{
    bool active = EmergencyManager::instance()->isActive();
    m_emergencyPanel->setVisible(active);

    if (!active)
    {
        // The status tabs go with the emergency. The filter applies no status once it has
        // ended, so clearing it changes nothing shown.
        m_filterBar->filter()->setContactStatusFilter(std::nullopt);
        m_filterTabs.first()->setChecked(true);
    }

    // The buttons of each expanded family again, with or without the emergency's
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        QModelIndex familyIndex = m_model->index(row, 0);
        if (m_treeView->isExpanded(familyIndex))
        {
            detachActionButtons(familyIndex);
            attachActionButtons(familyIndex);
        }
    }
}

void FamilyListView::onStatusFilterTabClicked()
{
    QPushButton* tab = qobject_cast<QPushButton*>(sender());
    if (!tab)
    {
        return;
    }

    int statusIndex = tab->property("statusIndex").toInt();
    onStatusFilterClicked(statusIndex);
}

void FamilyListView::onStatusFilterClicked(int statusIndex)
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

void FamilyListView::updateFilterTabCounts()
{
    EmergencyManager* manager = EmergencyManager::instance();
    if (!manager->isActive())
    {
        return;
    }

    int total = manager->totalFamilies();
    int ok = manager->countByStatus(EffectiveContactStatus::OK);
    int needsHelp = manager->countByStatus(EffectiveContactStatus::NeedsHelp);
    int unable = manager->countByStatus(EffectiveContactStatus::UnableToReach);
    int remaining = manager->countByStatus(EffectiveContactStatus::NotContacted);

    m_filterTabs[StatusFilterIndex::All]->setText(tr("All (%1)").arg(total));
    m_filterTabs[StatusFilterIndex::Remaining]->setText(tr("Remaining (%1)").arg(remaining));
    m_filterTabs[StatusFilterIndex::NeedsHelp]->setText(tr("Needs Help (%1)").arg(needsHelp));
    m_filterTabs[StatusFilterIndex::OK]->setText(tr("OK (%1)").arg(ok));
    m_filterTabs[StatusFilterIndex::UnableToReach]->setText(tr("Unable to Reach (%1)").arg(unable));
}

void FamilyListView::onTreeContextMenu(const QPoint& pos)
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

    QString taskIdStr = index.data(FamilyTreeModel::TaskIdRole).toString();
    if (taskIdStr.isEmpty())
    {
        return;
    }

    // Task rows exist only while an emergency is in progress
    TaskId taskId = TaskId::fromString(taskIdStr);
    const ResponseTask* task = EmergencyManager::instance()->response().findTask(taskId);
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

    // The menu ran an event loop, so each action finds the task again by its id
    if (chosen == editAction)
    {
        EmergencyActions::editTask(taskId, this);
    }
    else if (chosen == notifyAction)
    {
        EmergencyActions::notifyAssignee(taskId, this);
    }
    else if (chosen == resolveAction)
    {
        EmergencyActions::resolveTask(taskId, this);
    }
    else if (chosen == deleteAction)
    {
        EmergencyActions::deleteTask(taskId, this);
    }
}
