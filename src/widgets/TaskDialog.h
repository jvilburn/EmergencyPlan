#pragma once

#include "EmergencyResponse.h"

#include <QDialog>
#include <optional>

class QButtonGroup;
class QComboBox;
class QLineEdit;
class QRadioButton;
/// Dialog for adding or editing one family's task in the shown emergency response.
class TaskDialog : public QDialog
{
    Q_OBJECT

public:
    TaskDialog(const FamilyId& familyId, QWidget* parent);

    /// Pre-populate for editing an existing task.
    void setTask(const ResponseTask& task);

    /// The task as saved (valid after accept). An edit keeps the task's assignment, with its
    /// notification, while its assignee stays the same.
    std::optional<ResponseTask> result() const;

private slots:
    void onAccepted();
    void onCategoryActivated(int index);
    void onAssignmentChanged();

private:
    FamilyId m_familyId;
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
