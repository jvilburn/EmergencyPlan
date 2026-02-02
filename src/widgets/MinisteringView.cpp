#include "MinisteringView.h"
#include "MinisteringTabView.h"

#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

MinisteringView::MinisteringView(DocumentManager* docManager, QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(250);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Tab bar
    m_tabBar = new QTabBar();
    m_tabBar->addTab(tr("Elders Quorum"));
    m_tabBar->addTab(tr("Relief Society"));
    layout->addWidget(m_tabBar);

    // Tab views
    m_eqView = new MinisteringTabView(docManager, MinisteringOrg::EldersQuorum, this);
    m_rsView = new MinisteringTabView(docManager, MinisteringOrg::ReliefSociety, this);

    // Stack for switching
    m_stack = new QStackedWidget();
    m_stack->addWidget(m_eqView);
    m_stack->addWidget(m_rsView);
    layout->addWidget(m_stack, 1);

    // Connections
    connect(m_tabBar, &QTabBar::currentChanged,
            this, &MinisteringView::onTabChanged);

    connect(m_eqView, &MinisteringTabView::highlightChanged,
            this, &MinisteringView::highlightChanged);
    connect(m_rsView, &MinisteringTabView::highlightChanged,
            this, &MinisteringView::highlightChanged);

    // Initial state
    m_eqView->expandDistricts();
    m_rsView->expandDistricts();
}

void MinisteringView::onTabChanged(int index)
{
    m_currentOrg = (index == 0) ? MinisteringOrg::EldersQuorum : MinisteringOrg::ReliefSociety;
    m_stack->setCurrentIndex(index);
    emit highlightChanged();
}

HighlightInfo MinisteringView::highlightInfo() const
{
    return currentTabView()->highlightInfo();
}

QSet<QString> MinisteringView::visibleFamilyIds() const
{
    return {};  // Show all
}

MinisteringTabView* MinisteringView::currentTabView() const
{
    return (m_currentOrg == MinisteringOrg::EldersQuorum) ? m_eqView : m_rsView;
}
