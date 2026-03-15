#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>
#include <QModelIndex>
#include <QHash>
#include <optional>

class ActionButtonsWidget;
class DocumentManager;
class EmergencyManager;
class EmergencyProgressBar;
class FamilyTreeModel;
class Filter;
class FilterBar;
class QToolButton;
class SelectionPreservingTreeView;

/// Tree view for ward family list.
/// Owns FilterBar (which owns Filter) and FamilyTreeModel internally.
class WardListView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit WardListView(DocumentManager* documentManager,
                          EmergencyManager* emergencyManager,
                          QWidget* parent);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;

    std::optional<FamilyId> selectedFamilyId() const;
    void setSelectedFamilyId(const std::optional<FamilyId>& id);
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;
    QList<FamilyId> visibleFamilyIdsList() const;

    Filter* filter() const;

signals:
    void highlightChanged();
    void visibleFamiliesChanged(const QList<FamilyId>& familyIds);
    void editFamilyRequested(const FamilyId& familyId);
    void deleteFamilyRequested(const FamilyId& familyId);

private slots:
    void onSelectionChanged();
    void onModelReset();
    void onRowsRemoved(const QModelIndex& parent, int first, int last);
    void onRowsInserted(const QModelIndex& parent, int first, int last);
    void onItemExpanded(const QModelIndex& index);
    void onItemCollapsed(const QModelIndex& index);
    void onEmergencyStateChanged();
    void onStatusFilterTabClicked();
    void onStatusFilterClicked(int statusIndex);
    void updateFilterTabCounts();
    void onLogContactRequested(const FamilyId& familyId);
    void onAddTaskRequested(const FamilyId& familyId);
    void onEditTaskRequested(const FamilyId& familyId, const TaskId& taskId);
    void onNotifyTaskRequested(const FamilyId& familyId, const TaskId& taskId);
    void onResolveTaskRequested(const FamilyId& familyId, const TaskId& taskId);
    void onTreeContextMenu(const QPoint& pos);

private:
    void attachActionButtons(const QModelIndex& familyIndex);
    void detachActionButtons(const FamilyId& familyId);
    void setupEmergencyWidgets();

    DocumentManager* m_documentManager;
    EmergencyManager* m_emergencyManager;
    FilterBar* m_filterBar;
    FamilyTreeModel* m_model;
    SelectionPreservingTreeView* m_treeView;
    QHash<FamilyId, ActionButtonsWidget*> m_actionWidgets;

    // Emergency mode widgets (initially hidden)
    QWidget* m_emergencyPanel = nullptr;
    EmergencyProgressBar* m_progressBar = nullptr;
    QList<QToolButton*> m_filterTabs;
};
