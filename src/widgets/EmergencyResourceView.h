#pragma once

#include "FamilyMarkerProvider.h"
#include "DocumentChange.h"
#include "ResponseArea.h"

#include <QWidget>

class DocumentManager;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

/// EmergencyResourceView displays a 2-level tree of resources for a specific ResponseArea:
/// - EmergencyResource (top level) - shows "Name (N)" where N is assigned people count
/// - Person (under resource) - shows person's displayName
///
/// Includes a toolbar with Add, Edit, Delete buttons for discoverability.
class EmergencyResourceView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit EmergencyResourceView(DocumentManager* documentManager, ResponseArea area, QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onDocumentChanged(const DocumentChange& change);
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onSelectionChanged();

private:
    void rebuildTree();
    void updateButtonStates();
    void validateSelections();

    void addResource();
    void editResource();
    void deleteResource();
    void showSelectPeopleDialog(const QString& resourceId);
    void removePersonFromResource(const QString& resourceId, const QString& personId);

    // Constants for tree item data roles
    static constexpr int IdRole = Qt::UserRole;
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SecondaryIdRole = Qt::UserRole + 2;

    enum class ItemType
    {
        EmergencyResource,
        Person
    };

    DocumentManager* m_documentManager;
    ResponseArea m_area;
    QTreeWidget* m_tree;

    // Toolbar buttons
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;

    // Selection state
    QString m_selectedResourceId;
    QString m_selectedPersonId;
};
