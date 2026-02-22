#include "UnassignedTreeView.h"
#include "BaseTreeModel.h"
#include "ItemType.h"
#include "UnassignedMinisteringModel.h"
#include "SelectionPreservingTreeView.h"

#include <QVBoxLayout>

UnassignedTreeView::UnassignedTreeView(UnassignedMinisteringModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    m_tree = new SelectionPreservingTreeView(m_model, this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setIndentation(16);
    // NOTE: Height is managed by MinisteringTabView via headerExpansionChanged signal

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tree);

    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &UnassignedTreeView::onSelectionChanged);
    connect(m_tree, &QTreeView::expanded,
            this, &UnassignedTreeView::onTreeExpanded);
    connect(m_tree, &QTreeView::collapsed,
            this, &UnassignedTreeView::onTreeCollapsed);
    connect(m_model, &QAbstractItemModel::modelAboutToBeReset,
            this, &UnassignedTreeView::onModelAboutToBeReset);
    connect(m_model, &QAbstractItemModel::modelReset,
            this, &UnassignedTreeView::onModelReset);
}

void UnassignedTreeView::onSelectionChanged()
{
    emit highlightChanged();
}

void UnassignedTreeView::onTreeExpanded(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    if (type == ItemType::UnassignedHeader)
    {
        emit headerExpansionChanged(true);
    }
    else if (type == ItemType::MinisteredFamily || type == ItemType::MinisteredSister)
    {
        m_model->loadContactDetails(index);
    }
}

void UnassignedTreeView::onTreeCollapsed(const QModelIndex& index)
{
    if (m_model->itemTypeAt(index) == ItemType::UnassignedHeader)
    {
        emit headerExpansionChanged(false);
    }
}

void UnassignedTreeView::onModelAboutToBeReset()
{
    // Save tree node expansion state - header is row 0 at root
    QModelIndex headerIndex = m_model->index(0, 0);
    m_wasExpanded = headerIndex.isValid() && m_tree->isExpanded(headerIndex);
}

void UnassignedTreeView::onModelReset()
{
    // Restore tree node expansion state
    QModelIndex headerIndex = m_model->index(0, 0);
    if (headerIndex.isValid() && m_wasExpanded)
    {
        m_tree->expand(headerIndex);
        // expand() triggers onTreeExpanded which emits headerExpansionChanged
    }

    // Notify parent that unassigned count may have changed (for visibility updates)
    emit unassignedCountChanged();
}

bool UnassignedTreeView::hasUnassigned() const
{
    return m_model->hasUnassigned();
}

HighlightInfo UnassignedTreeView::highlightInfo() const
{
    // Delegate to model - it computes family associations
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}

QSet<FamilyId> UnassignedTreeView::visibleFamilyIds() const
{
    return {};  // Show all
}
