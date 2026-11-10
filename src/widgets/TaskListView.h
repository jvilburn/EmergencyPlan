#pragma once

#include "FamilyMarkerProvider.h"
#include "Id.h"

#include <QWidget>
#include <optional>

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

    /// Saves the selected task and the task at the top before the table rebuilds, and
    /// selects and scrolls to them after.
    void onModelAboutToBeReset();
    void onModelReset();

private:
    /// The row showing task, or -1.
    int rowOf(const TaskId& task) const;

    /// The task at index's row, or none for an invalid index.
    std::optional<TaskId> taskAt(const QModelIndex& index) const;

    TaskListModel* m_model;
    QTreeView* m_treeView;
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;
    QPushButton* m_notifyButton;
    std::optional<TaskId> m_savedTask;
    std::optional<TaskId> m_topTask;
};
