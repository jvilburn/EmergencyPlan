#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class FilterBar;
class NeedsModel;
class QModelIndex;
class QPushButton;
class SelectionPreservingTreeView;

/// NeedsSubView displays a 2-level tree of special needs.
/// Owns FilterBar (which owns Filter) and NeedsModel internally.
class NeedsSubView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit NeedsSubView(QWidget* parent);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeExpanded(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);
    void editNeedFromContextMenu();
    void deleteNeedFromContextMenu();
    void editSelectedNeed();
    void deleteSelectedNeed();

private:
    void addNeed();
    void showNeedDialog(const std::optional<PersonId>& personId);
    void deleteNeed(const PersonId& personId, const FamilyId& familyId);
    void updateButtonStates();

    FilterBar* m_filterBar;
    NeedsModel* m_model;
    SelectionPreservingTreeView* m_tree;

    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;

    // Context menu state
    std::optional<PersonId> m_contextPersonId;
    std::optional<FamilyId> m_contextFamilyId;
};
