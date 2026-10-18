#pragma once

#include <QWidget>
#include "Person.h"

class QLineEdit;
class QCheckBox;
class QComboBox;
class QSpinBox;
class CallingsEditor;

/// Widget for editing the fields of a Person that the form shows.
class MemberEditor : public QWidget
{
    Q_OBJECT

public:
    explicit MemberEditor(QWidget* parent);

    void setPerson(const Person& person);

    /// The person given to setPerson, with each field the form shows taken from its widget
    /// where the widget no longer holds what setPerson put there. Fields the form does not
    /// show, and fields the user left alone, are the given person's.
    Person person() const;

    const PersonId& personId() const { return m_person.id(); }

signals:
    void dataChanged();
    void removeRequested();

private:
    void setupUi();
    Birthday birthdayFromForm() const;
    std::optional<Gender> genderFromForm() const;

    Person m_person;

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
