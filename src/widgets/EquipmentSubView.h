#pragma once

#include "MapHighlightProvider.h"

#include <QWidget>

class DocumentManager;
class QTreeWidget;
class QTreeWidgetItem;

/// EquipmentSubView displays a 3-level tree hierarchy of equipment:
/// - Category (top level) - shows "CategoryName (N)" where N is total families
/// - Equipment (under category) - shows "EquipmentName (N)" where N is families with that equipment
/// - Family (under equipment) - shows family's displayName
///
/// Implements MapHighlightProvider to highlight families with selected equipment/categories.
class EquipmentSubView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT

public:
    explicit EquipmentSubView(DocumentManager* documentManager, QWidget* parent = nullptr);

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
    void showAddCategoryDialog();
    void showAddEquipmentDialog(const QString& categoryId);
    void showRenameDialog(QTreeWidgetItem* item);
    void showSelectFamiliesDialog(const QString& equipmentId);
    void deleteItem(QTreeWidgetItem* item);
    void removeFamilyFromEquipment(const QString& equipmentId, const QString& familyId);

    // Constants for tree item data roles
    static constexpr int IdRole = Qt::UserRole;
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SecondaryIdRole = Qt::UserRole + 2;

    enum class ItemType
    {
        Category,
        Equipment,
        Family
    };

    DocumentManager* m_documentManager;
    QTreeWidget* m_tree;

    // Selection state
    QString m_selectedCategoryId;
    QString m_selectedEquipmentId;
    QString m_selectedFamilyId;
};
