#pragma once

#include "EmergencyResponse.h"

#include <QDialog>
#include <optional>

class QButtonGroup;
class QComboBox;
class QLineEdit;
class QRadioButton;

/// Dialog for logging a contact attempt on one family during an emergency: the method, who
/// made the contact, the outcome when there is one, and notes.
class ContactAttemptDialog : public QDialog
{
    Q_OBJECT

public:
    ContactAttemptDialog(const FamilyId& familyId, QWidget* parent);

    std::optional<ContactAttempt> result() const;

private slots:
    void onAccepted();

private:
    FamilyId m_familyId;
    QButtonGroup* m_methodGroup = nullptr;
    QComboBox* m_whoCombo = nullptr;
    QRadioButton* m_okRadio = nullptr;
    QRadioButton* m_unableToReachRadio = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    std::optional<ContactAttempt> m_result;
};
