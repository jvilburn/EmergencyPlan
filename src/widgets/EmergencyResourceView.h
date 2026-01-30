#pragma once

#include "FamilyMarkerProvider.h"
#include "ResponseArea.h"

#include <QWidget>

class DocumentManager;
class EmergencyResourceModel;
class QTreeView;
class QModelIndex;
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
    void onSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    void onTreeDoubleClicked(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);

private:
    void updateButtonStates();

    void addResource();
    void editResource();
    void deleteResource();
    void showSelectPeopleDialog(const QString& resourceId);
    void removePersonFromResource(const QString& resourceId, const QString& personId);

    // Helpers for current selection
    QString selectedResourceId() const;

    DocumentManager* m_documentManager;
    ResponseArea m_area;
    EmergencyResourceModel* m_model;
    QTreeView* m_tree;

    // Toolbar buttons
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;
};
