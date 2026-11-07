#pragma once

#include "EmergencyResponse.h"

#include <QDialog>
#include <optional>

class QButtonGroup;
class QComboBox;
class QLineEdit;
class QRadioButton;

/// Dialog for logging a contact attempt on one family during an emergency: the method, who
/// made the contact, the outcome when there is one, and notes. The attempt names the family
/// as it stood when the dialog opened.
class ContactAttemptDialog : public QDialog
{
    Q_OBJECT

public:
    ContactAttemptDialog(const Family& family, QWidget* parent);

    std::optional<ContactAttempt> result() const;

private slots:
    void onAccepted();

private:
    Family m_family;
    QButtonGroup* m_methodGroup = nullptr;
    QComboBox* m_whoCombo = nullptr;
    QRadioButton* m_okRadio = nullptr;
    QRadioButton* m_unableToReachRadio = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    std::optional<ContactAttempt> m_result;
};
