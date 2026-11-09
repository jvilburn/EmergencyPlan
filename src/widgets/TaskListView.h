#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class QTreeView;
class QPushButton;
class TaskListModel;

/// The Tasks tab: every task in the emergency in progress, with buttons that run their
/// actions through EmergencyActions.
class TaskListView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit TaskListView(QWidget* parent);

    // FamilyMarkerProvider
    HighlightInfo highlightInfo() const override;
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;

signals:
    void highlightChanged();

private slots:
    void onAddTask();
    void onEditTask();
    void onDeleteTask();
    void onNotifyTask();
    void onSelectionChanged();

private:
    TaskListModel* m_model;
    QTreeView* m_treeView;
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;
    QPushButton* m_notifyButton;
};
