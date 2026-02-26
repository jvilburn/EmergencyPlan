#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class MinisteringModel;
class SelectionPreservingTreeView;
class QModelIndex;

/// Tree view for assigned ministering (districts, companionships, ministers, families).
/// Takes model pointer - delegates highlightInfo() to model's relatedFamiliesAt().
class MinisteringTreeView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit MinisteringTreeView(MinisteringModel* model, QWidget* parent = nullptr);

    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;

    void expandDistricts();

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeExpanded(const QModelIndex& index);

private:
    MinisteringModel* m_model;
    SelectionPreservingTreeView* m_tree;
};
