#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class DocumentManager;
class NeedsModel;
class QTreeView;
class QModelIndex;

/// NeedsSubView displays a flat list of special needs (persons/families with special needs).
///
/// Each item displays: "DisplayName - note" where DisplayName is the person or family name.
///
/// Implements FamilyMarkerProvider to highlight families of selected persons or directly selected families.
class NeedsSubView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit NeedsSubView(DocumentManager* documentManager, QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    void onContextMenu(const QPoint& pos);

private:
    void showAddNeedDialog();
    void showEditNeedDialog(const QString& personId, const QString& familyId);
    void deleteNeed(const QString& personId, const QString& familyId);

    DocumentManager* m_documentManager;
    NeedsModel* m_model;
    QTreeView* m_tree;
};
