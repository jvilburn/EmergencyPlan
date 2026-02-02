#include "MinisteringTabView.h"
#include "MinisteringModel.h"
#include "UnassignedMinisteringModel.h"
#include "MinisteringTreeView.h"
#include "UnassignedTreeView.h"

#include <QFontMetrics>
#include <QVBoxLayout>

MinisteringTabView::MinisteringTabView(DocumentManager* documentManager,
                                         MinisteringOrg org,
                                         QWidget* parent)
    : QWidget(parent)
{
    // Create models with org - models own the org context
    m_mainModel = new MinisteringModel(documentManager, org, this);
    m_unassignedModel = new UnassignedMinisteringModel(documentManager, org, this);

    // Pass models to tree views - views don't need org
    m_mainView = new MinisteringTreeView(m_mainModel, this);
    m_unassignedView = new UnassignedTreeView(m_unassignedModel, this);

    // Set initial collapsed height
    m_unassignedView->setFixedHeight(collapsedTreeHeight());

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
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

QSet<QString> MinisteringTabView::visibleFamilyIds() const
{
    return {};  // Show all
}

void MinisteringTabView::expandDistricts()
{
    m_mainView->expandDistricts();
}

void MinisteringTabView::updateUnassignedVisibility()
{
    m_unassignedView->setVisible(m_unassignedView->hasUnassigned());
}
