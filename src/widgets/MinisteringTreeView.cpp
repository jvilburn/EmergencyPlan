#include "MinisteringTreeView.h"
#include "BaseTreeModel.h"
#include "ItemType.h"
#include "MinisteringModel.h"
#include "SelectionPreservingTreeView.h"

#include <QVBoxLayout>

MinisteringTreeView::MinisteringTreeView(MinisteringModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    m_tree = new SelectionPreservingTreeView(m_model, this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setIndentation(16);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tree);

    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &MinisteringTreeView::onSelectionChanged);
    connect(m_tree, &QTreeView::expanded,
            this, &MinisteringTreeView::onTreeExpanded);
}

void MinisteringTreeView::onSelectionChanged()
{
    emit highlightChanged();
}

void MinisteringTreeView::onTreeExpanded(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    if (type == ItemType::Companionship)
    {
        // Expand section headers when companionship is expanded
        int rowCount = m_model->rowCount(index);
        for (int i = 0; i < rowCount; ++i)
        {
            QModelIndex childIndex = m_model->index(i, 0, index);
            ItemType childType = m_model->itemTypeAt(childIndex);
            if (childType == ItemType::SectionHeader)
            {
                m_tree->expand(childIndex);
            }
        }
    }
    else if (type == ItemType::Minister
             || type == ItemType::MinisteredFamily
             || type == ItemType::MinisteredSister)
    {
        // Load contact info on expand
        m_model->loadContactDetails(index);
    }
}

void MinisteringTreeView::expandDistricts()
{
    m_tree->expandToDepth(0);
}

HighlightInfo MinisteringTreeView::highlightInfo() const
{
    // Delegate to model - it computes family associations
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}

QSet<FamilyId> MinisteringTreeView::visibleFamilyIds() const
{
    return {};  // Show all
}

void MinisteringTreeView::clearSelection()
{
    m_tree->clearSelection();
}

void MinisteringTreeView::selectFamily(const FamilyId& familyId)
{
    QModelIndex idx = m_model->indexForFamilyId(familyId);
    if (!idx.isValid())
    {
        idx = m_model->indexForMinisterByFamilyId(familyId);
    }
    if (idx.isValid())
    {
        m_tree->setCurrentIndex(idx);
        m_tree->scrollTo(idx);
    }
}
