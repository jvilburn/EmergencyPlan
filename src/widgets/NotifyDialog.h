#pragma once

#include "EmergencyResponse.h"

#include <QDialog>
#include <optional>

class QButtonGroup;
class QLineEdit;

/// Dialog for recording notification of a task assignee.
class NotifyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NotifyDialog(QWidget* parent);

    std::optional<TaskNotification> result() const;

private slots:
    void onAccepted();

private:
    QButtonGroup* m_methodGroup = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    std::optional<TaskNotification> m_result;
};
