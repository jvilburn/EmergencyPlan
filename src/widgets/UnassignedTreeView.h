#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class UnassignedMinisteringModel;
class SelectionPreservingTreeView;
class QModelIndex;

/// Tree view for unassigned families/sisters.
/// Takes model pointer - delegates highlightInfo() to model's relatedFamiliesAt().
/// Preserves tree node expansion state across model resets.
/// NOTE: Widget HEIGHT is managed by MinisteringTabView via headerExpansionChanged signal.
class UnassignedTreeView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit UnassignedTreeView(UnassignedMinisteringModel* model, QWidget* parent = nullptr);

    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;

    bool hasUnassigned() const;

signals:
    void highlightChanged();
    void unassignedCountChanged();  // Emitted after model reset (for visibility updates)
    void headerExpansionChanged(bool isExpanded);  // For tab view to manage height

private slots:
    void onSelectionChanged();
    void onTreeExpanded(const QModelIndex& index);
    void onTreeCollapsed(const QModelIndex& index);
    void onModelAboutToBeReset();
    void onModelReset();

private:
    UnassignedMinisteringModel* m_model;
    SelectionPreservingTreeView* m_tree;
    bool m_wasExpanded = false;  // Track tree node expansion state for preservation
};
