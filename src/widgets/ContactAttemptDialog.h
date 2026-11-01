#pragma once

#include "EmergencyResponse.h"

#include <QDialog>
#include <optional>

class QButtonGroup;
class QComboBox;
class QLineEdit;
/// Dialog for logging a contact attempt on one family during an emergency: the method, who
/// made the contact, and notes.
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
    QLineEdit* m_notesEdit = nullptr;
    std::optional<ContactAttempt> m_result;
};
