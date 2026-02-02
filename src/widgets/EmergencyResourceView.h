#pragma once

#include "FamilyMarkerProvider.h"
#include "ResponseArea.h"

#include <QWidget>

class DocumentManager;
class EmergencyResourceModel;
class FilterBar;
class QModelIndex;
class QPushButton;
class SelectionPreservingTreeView;

/// EmergencyResourceView displays a 3-level tree of resources.
/// Owns FilterBar (which owns Filter) and EmergencyResourceModel internally.
/// Includes a toolbar with Add, Edit, Delete buttons.
class EmergencyResourceView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit EmergencyResourceView(DocumentManager* documentManager,
                                    ResponseArea area,
                                    QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeDoubleClicked(const QModelIndex& index);
    void onTreeExpanded(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);
    void expandResources();
    void selectPeopleFromContextMenu();
    void removePersonFromContextMenu();

private:
    void updateButtonStates();

    void addResource();
    void editResource();
    void deleteResource();
    void showSelectPeopleDialog(const QString& resourceId);
    void removePersonFromResource(const QString& resourceId, const QString& personId);

    QString selectedResourceId() const;

    DocumentManager* m_documentManager;
    FilterBar* m_filterBar;
    EmergencyResourceModel* m_model;
    SelectionPreservingTreeView* m_tree;

    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;

    QString m_contextResourceId;
    QString m_contextPersonId;
};
