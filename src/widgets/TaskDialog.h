#pragma once

#include "EmergencyResponse.h"

#include <QDialog>
#include <optional>

class QButtonGroup;
class QComboBox;
class QLineEdit;
class QRadioButton;
/// Dialog for adding or editing a response task during an emergency.
class TaskDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TaskDialog(QWidget* parent);

    /// Pre-populate for editing an existing task.
    void setTask(const ResponseTask& task);

    /// Returns the constructed task (valid after accept).
    std::optional<ResponseTask> result() const;

private slots:
    void onAccepted();
    void onCategoryActivated(int index);
    void onAssignmentChanged();

private:
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
