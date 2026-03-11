#include "WardListView.h"
#include "ActionButtonsWidget.h"
#include "Document.h"
#include "DocumentManager.h"
#include "Family.h"
#include "FamilyTreeModel.h"
#include "FilterBar.h"
#include "SelectionPreservingTreeView.h"

#include <QHeaderView>
#include <QVBoxLayout>

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

    // Connect model signals
    connect(m_model, &QAbstractItemModel::modelReset,
            this, &WardListView::onModelReset);
    connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, &WardListView::onRowsRemoved);
    connect(m_model, &QAbstractItemModel::rowsInserted,
            this, &WardListView::onRowsInserted);

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
