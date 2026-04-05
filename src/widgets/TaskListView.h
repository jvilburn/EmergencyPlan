#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class QTreeView;
class QPushButton;
class EmergencyManager;
class TaskListModel;

class TaskListView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit TaskListView(EmergencyManager* emergencyManager,
                          QWidget* parent);

    void rebuild();

    // FamilyMarkerProvider
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;

signals:
    void highlightChanged();

private slots:
    void onAddTask();
    void onEditTask();
    void onDeleteTask();
    void onSelectionChanged();

private:
    EmergencyManager* m_emergencyManager;
    TaskListModel* m_model;
    QTreeView* m_treeView;
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;
};
