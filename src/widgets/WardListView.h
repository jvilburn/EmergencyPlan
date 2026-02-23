#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>
#include <QModelIndex>
#include <QHash>
#include <optional>

class ActionButtonsWidget;
class DocumentManager;
class FamilyTreeModel;
class Filter;
class FilterBar;
class SelectionPreservingTreeView;

/// Tree view for ward family list.
/// Owns FilterBar (which owns Filter) and FamilyTreeModel internally.
class WardListView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit WardListView(DocumentManager* documentManager,
                          QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;

    std::optional<FamilyId> selectedFamilyId() const;
    void setSelectedFamilyId(const std::optional<FamilyId>& id);
    void clearSelection();
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

private:
    void attachActionButtons(const QModelIndex& familyIndex);
    void detachActionButtons(const FamilyId& familyId);

    DocumentManager* m_documentManager;
    FilterBar* m_filterBar;
    FamilyTreeModel* m_model;
    SelectionPreservingTreeView* m_treeView;
    QHash<FamilyId, ActionButtonsWidget*> m_actionWidgets;
};
