#pragma once

#include "FamilyMarkerProvider.h"
#include "ItemType.h"

#include <QWidget>

class MinisteringTabView;
class QStackedWidget;
class QTabBar;

/// Container for EQ/RS ministering tabs. Delegates to current tab.
class MinisteringView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit MinisteringView(QWidget* parent);

    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;

signals:
    void highlightChanged();

private slots:
    void onTabChanged(int index);

private:
    MinisteringTabView* currentTabView() const;

    QTabBar* m_tabBar;
    QStackedWidget* m_stack;
    MinisteringTabView* m_eqView;
    MinisteringTabView* m_rsView;
    MinisteringOrg m_currentOrg = MinisteringOrg::EldersQuorum;
};
