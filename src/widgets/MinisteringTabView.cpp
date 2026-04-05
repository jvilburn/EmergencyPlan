#include "MinisteringTabView.h"
#include "FilterBar.h"
#include "MinisteringModel.h"
#include "UnassignedMinisteringModel.h"
#include "MinisteringTreeView.h"
#include "UnassignedTreeView.h"

#include <QFontMetrics>
#include <QVBoxLayout>

MinisteringTabView::MinisteringTabView(EmergencyManager* emergencyManager,
                                         MinisteringOrg org,
                                         QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // FilterBar shared between both trees
    m_filterBar = new FilterBar(this);
    layout->addWidget(m_filterBar);

    // Create models with filter from FilterBar
    m_mainModel = new MinisteringModel(emergencyManager, m_filterBar->filter(), org, this);
    m_unassignedModel = new UnassignedMinisteringModel(m_filterBar->filter(), org, this);

    // Pass models to tree views - views don't need org
    m_mainView = new MinisteringTreeView(m_mainModel, this);
    m_unassignedView = new UnassignedTreeView(m_unassignedModel, this);

    // Set initial collapsed height
    m_unassignedView->setFixedHeight(collapsedTreeHeight());

    layout->addWidget(m_unassignedView);
    layout->addWidget(m_mainView, 1);

    // Highlight delegation
    connect(m_mainView, &MinisteringTreeView::highlightChanged,
            this, &MinisteringTabView::onMainHighlightChanged);
    connect(m_unassignedView, &UnassignedTreeView::highlightChanged,
            this, &MinisteringTabView::onUnassignedHighlightChanged);

    // Visibility (hide when count == 0)
    connect(m_unassignedView, &UnassignedTreeView::unassignedCountChanged,
            this, &MinisteringTabView::updateUnassignedVisibility);

    // HEIGHT IS HANDLED BY TAB VIEW
    connect(m_unassignedView, &UnassignedTreeView::headerExpansionChanged,
            this, &MinisteringTabView::onHeaderExpansionChanged);

    updateUnassignedVisibility();
}

void MinisteringTabView::onMainHighlightChanged()
{
    m_activeProvider = m_mainView;
    emit highlightChanged();
}

void MinisteringTabView::onUnassignedHighlightChanged()
{
    m_activeProvider = m_unassignedView;
    emit highlightChanged();
}

void MinisteringTabView::onHeaderExpansionChanged(bool isExpanded)
{
    m_unassignedView->setFixedHeight(isExpanded ? expandedTreeHeight() : collapsedTreeHeight());
}

int MinisteringTabView::collapsedTreeHeight() const
{
    // Height for single row (header only) + margins
    // Using font metrics rather than sizeHintForRow() since tree may be empty
    QFontMetrics fm(font());
    int rowHeight = fm.height() + 8;  // Text height + tree item padding
    return rowHeight + 16;  // Widget margins
}

int MinisteringTabView::expandedTreeHeight() const
{
    // Limit to ~8 rows to avoid taking too much vertical space from main tree.
    // Using font metrics rather than sizeHintForRow() because we want a fixed
    // maximum regardless of actual content count.
    QFontMetrics fm(font());
    int rowHeight = fm.height() + 8;
    return rowHeight * 8 + 16;
}

HighlightInfo MinisteringTabView::highlightInfo() const
{
    if (m_activeProvider)
    {
        return m_activeProvider->highlightInfo();
    }
    return {};
}

QSet<FamilyId> MinisteringTabView::visibleFamilyIds() const
{
    return {};  // Show all
}

void MinisteringTabView::clearSelection()
{
    m_mainView->clearSelection();
    m_unassignedView->clearSelection();
}

void MinisteringTabView::selectFamily(const FamilyId& familyId)
{
    // 1. Narrow within current selection: if the family is already highlighted,
    //    select that specific row in the active tree
    if (m_activeProvider)
    {
        HighlightInfo info = m_activeProvider->highlightInfo();
        if (info.allHighlightedIds().contains(familyId))
        {
            // Clear the opposite view to avoid dual-selection
            if (m_activeProvider == m_mainView)
            {
                m_unassignedView->clearSelection();
            }
            else
            {
                m_mainView->clearSelection();
            }
            m_activeProvider->selectFamily(familyId);
            return;
        }
    }

    // 2. Ministered-to family/sister in main tree
    if (m_mainModel->indexForFamilyId(familyId).isValid())
    {
        m_unassignedView->clearSelection();
        m_mainView->selectFamily(familyId);
        return;
    }

    // 3. Minister in main tree
    if (m_mainModel->indexForMinisterByFamilyId(familyId).isValid())
    {
        m_unassignedView->clearSelection();
        m_mainView->selectFamily(familyId);
        return;
    }

    // 4. Unassigned
    if (m_unassignedView->hasFamily(familyId))
    {
        m_mainView->clearSelection();
        m_unassignedView->selectFamily(familyId);
    }
}

void MinisteringTabView::expandDistricts()
{
    m_mainView->expandDistricts();
}

void MinisteringTabView::updateUnassignedVisibility()
{
    m_unassignedView->setVisible(m_unassignedView->hasUnassigned());
}
