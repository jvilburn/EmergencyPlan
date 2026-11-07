#pragma once

#include "EmergencyResponse.h"

#include <QDialog>
#include <optional>

class QButtonGroup;
class QComboBox;
class QLineEdit;
class QRadioButton;

/// Dialog for adding or editing one family's task in the emergency in progress.
class TaskDialog : public QDialog
{
    Q_OBJECT

public:
    /// Adds a task for family, naming the family as it stood when the dialog opened.
    TaskDialog(const Family& family, QWidget* parent);

    /// Edits task, showing its category and its assignee even when the emergency or the
    /// ward no longer lists them.
    TaskDialog(const ResponseTask& task, QWidget* parent);

    /// The task as saved (valid after accept). An edit keeps the task's assignment, with its
    /// notification and the name it recorded, while its assignee stays the same.
    std::optional<ResponseTask> result() const;

private slots:
    void onAccepted();
    void onCategoryActivated(int index);
    void onAssignmentChanged();

private:
    /// The dialog's widgets, with no task and no family.
    explicit TaskDialog(QWidget* parent);

    /// Lists category before "+ Add new...", and gives its index.
    int addCategory(const QString& category);

    /// Selects the entry carrying the assignee's id in combo, adding one with the name the
    /// assignment recorded when there is none.
    void selectAssignee(QComboBox* combo, const TaskAssignment& assignment);

    /// The family a new task is for; none for an edit.
    std::optional<Family> m_family;
    QComboBox* m_categoryCombo = nullptr;
    QLineEdit* m_descriptionEdit = nullptr;
    QButtonGroup* m_assignGroup = nullptr;
    QRadioButton* m_unassignedRadio = nullptr;
    QRadioButton* m_teamRadio = nullptr;
    QRadioButton* m_personRadio = nullptr;
    QComboBox* m_teamCombo = nullptr;
    QComboBox* m_personCombo = nullptr;
    std::optional<ResponseTask> m_result;
    std::optional<ResponseTask> m_originalTask;
};
