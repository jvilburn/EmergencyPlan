#pragma once

#include "FamilyMarkerProvider.h"
#include "ItemType.h"

#include <QWidget>

class EmergencyManager;
class FilterBar;
class MinisteringModel;
class UnassignedMinisteringModel;
class MinisteringTreeView;
class UnassignedTreeView;

/// Tab content - creates models, owns tree views for one org.
/// Delegates highlightInfo() to whichever tree view has selection.
/// IMPORTANT: Manages UnassignedTreeView height (connects to headerExpansionChanged).
class MinisteringTabView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit MinisteringTabView(EmergencyManager* emergencyManager,
                                 MinisteringOrg org,
                                 QWidget* parent);

    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;

    void expandDistricts();

signals:
    void highlightChanged();

private slots:
    void onMainHighlightChanged();
    void onUnassignedHighlightChanged();
    void updateUnassignedVisibility();
    void onHeaderExpansionChanged(bool isExpanded);

private:
    int collapsedTreeHeight() const;
    int expandedTreeHeight() const;

    // FilterBar shared between both trees
    FilterBar* m_filterBar;

    // Models owned by tab view
    MinisteringModel* m_mainModel;
    UnassignedMinisteringModel* m_unassignedModel;

    // Views take model pointers
    MinisteringTreeView* m_mainView;
    UnassignedTreeView* m_unassignedView;
    FamilyMarkerProvider* m_activeProvider = nullptr;
};
