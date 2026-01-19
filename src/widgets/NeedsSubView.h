#pragma once

#include "MapHighlightProvider.h"

#include <QWidget>

class DocumentManager;
class QTreeWidget;
class QTreeWidgetItem;

/// NeedsSubView displays a flat list of special needs (persons/families with special needs).
/// Unlike SkillsSubView/EquipmentSubView, this is NOT a 3-level hierarchy - just a simple list.
///
/// Each item displays: "DisplayName - note" where DisplayName is the person or family name.
///
/// Implements MapHighlightProvider to highlight families of selected persons or directly selected families.
class NeedsSubView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT

public:
    explicit NeedsSubView(DocumentManager* documentManager, QWidget* parent = nullptr);

    // MapHighlightProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onDocumentChanged();
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);

private:
    void rebuildTree();
    void showAddNeedDialog();
    void showEditNeedDialog(const QString& personId, const QString& familyId);
    void deleteNeed(const QString& personId, const QString& familyId);

    // Constants for tree item data roles
    static constexpr int PersonIdRole = Qt::UserRole;
    static constexpr int FamilyIdRole = Qt::UserRole + 1;

    DocumentManager* m_documentManager;
    QTreeWidget* m_tree;

    // Selection state
    QString m_selectedPersonId;
    QString m_selectedFamilyId;
};
