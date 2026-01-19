#pragma once

#include "FamilyMarkerProvider.h"
#include "DocumentChange.h"

#include <QWidget>

class DocumentManager;
class QTreeWidget;
class QTreeWidgetItem;

/// SkillsSubView displays a 3-level tree hierarchy of skills:
/// - Category (top level) - shows "CategoryName (N)" where N is total persons
/// - Skill (under category) - shows "SkillName (N)" where N is persons with that skill
/// - Person (under skill) - shows person's displayName
///
/// Implements FamilyMarkerProvider to highlight families of selected persons/skills/categories.
class SkillsSubView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit SkillsSubView(DocumentManager* documentManager, QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onDocumentChanged(const DocumentChange& change);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);

private:
    void rebuildTree();
    void validateSelections();
    void showAddCategoryDialog();
    void showAddSkillDialog(const QString& categoryId);
    void showRenameDialog(QTreeWidgetItem* item);
    void showSelectPeopleDialog(const QString& skillId);
    void deleteItem(QTreeWidgetItem* item);
    void removePersonFromSkill(const QString& skillId, const QString& personId);

    // Constants for tree item data roles
    static constexpr int IdRole = Qt::UserRole;
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SecondaryIdRole = Qt::UserRole + 2;

    enum class ItemType
    {
        Category,
        Skill,
        Person
    };

    DocumentManager* m_documentManager;
    QTreeWidget* m_tree;

    // Selection state
    QString m_selectedCategoryId;
    QString m_selectedSkillId;
    QString m_selectedPersonId;
};
