#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class DocumentManager;
class FilterBar;
class NeedsModel;
class QModelIndex;
class SelectionPreservingTreeView;

/// NeedsSubView displays a 2-level tree of special needs.
/// Owns FilterBar (which owns Filter) and NeedsModel internally.
class NeedsSubView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit NeedsSubView(DocumentManager* documentManager,
                          QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void clearSelection() override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeExpanded(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);

private:
    void showAddNeedDialog();
    void showEditNeedDialog(const PersonId& personId, const FamilyId& familyId);
    void deleteNeed(const PersonId& personId, const FamilyId& familyId);

    DocumentManager* m_documentManager;
    FilterBar* m_filterBar;
    NeedsModel* m_model;
    SelectionPreservingTreeView* m_tree;
};
