#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>
#include <QModelIndex>
#include <optional>

class EmergencyProgressBar;
class FamilyTreeModel;
class Filter;
class FilterBar;
class QPushButton;
class SelectionPreservingTreeView;

/// Tree view of the ward's families.
/// Owns FilterBar (which owns Filter) and FamilyTreeModel internally.
class FamilyListView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit FamilyListView(QWidget* parent);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    std::optional<QSet<FamilyId>> visibleFamilyIds() const override;
    QString familyStatusIcon(const FamilyId& familyId) const override;

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
    void onItemExpanded(const QModelIndex& index);
    void onItemCollapsed(const QModelIndex& index);
    void onEmergencyStateChanged();
    void onStatusFilterTabClicked();
    void onStatusFilterClicked(int statusIndex);
    void updateFilterTabCounts();
    void onTreeContextMenu(const QPoint& pos);

private:
    /// The family's Actions row, where its buttons go.
    QModelIndex actionsIndex(const QModelIndex& familyIndex) const;

    /// Puts the family's buttons on its Actions row, unless they are there already.
    void attachActionButtons(const QModelIndex& familyIndex);
    void detachActionButtons(const QModelIndex& familyIndex);
    void setupEmergencyWidgets();

    FilterBar* m_filterBar;
    FamilyTreeModel* m_model;
    SelectionPreservingTreeView* m_treeView;

    // Emergency mode widgets (initially hidden)
    QWidget* m_emergencyPanel = nullptr;
    EmergencyProgressBar* m_progressBar = nullptr;
    QList<QPushButton*> m_filterTabs;
};
