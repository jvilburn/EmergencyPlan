#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class DocumentManager;
class NeedsModel;
class QModelIndex;
class SelectionPreservingTreeView;

/// NeedsSubView displays a 2-level tree of special needs.
/// Takes model pointer - parent creates model.
class NeedsSubView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit NeedsSubView(NeedsModel* model,
                          DocumentManager* documentManager,
                          QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeExpanded(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);

private:
    void showAddNeedDialog();
    void showEditNeedDialog(const QString& personId, const QString& familyId);
    void deleteNeed(const QString& personId, const QString& familyId);

    DocumentManager* m_documentManager;
    NeedsModel* m_model;
    SelectionPreservingTreeView* m_tree;
};
