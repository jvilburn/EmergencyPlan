#pragma once

#include <QWidget>
#include "Person.h"

class QLineEdit;
class QCheckBox;
class QComboBox;
class QSpinBox;
class CallingsEditor;

/// Widget for editing all fields of a Person.
class MemberEditor : public QWidget
{
    Q_OBJECT

public:
    explicit MemberEditor(QWidget* parent);

    void setPerson(const Person& person);
    Person person() const;

    /// Returns the original person ID (preserved across edits).
    const PersonId& personId() const { return m_personId; }

signals:
    void dataChanged();
    void removeRequested();

private:
    void setupUi();

    PersonId m_personId;

    // Name
    QLineEdit* m_surnameEdit;
    QLineEdit* m_givenNamesEdit;

    // Parent/Gender
    QCheckBox* m_parentCheck;
    QComboBox* m_genderCombo;

    // Birthday
    QSpinBox* m_daySpinner;
    QComboBox* m_monthCombo;
    QLineEdit* m_yearEdit;

    // Contact
    QLineEdit* m_phoneEdit;
    QLineEdit* m_altPhoneEdit;
    QLineEdit* m_emailEdit;

    // Callings
    CallingsEditor* m_callingsEditor;
};
