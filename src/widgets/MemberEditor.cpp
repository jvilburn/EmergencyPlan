#include "MemberEditor.h"
#include "CallingsEditor.h"
#include "Name.h"
#include "Phone.h"
#include "Email.h"
#include "Birthday.h"
#include "Gender.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>

MemberEditor::MemberEditor(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void MemberEditor::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(12);

    // Name section
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(tr("Surname:"), this));
    m_surnameEdit = new QLineEdit(this);
    nameLayout->addWidget(m_surnameEdit, 1);
    nameLayout->addWidget(new QLabel(tr("Given:"), this));
    m_givenNamesEdit = new QLineEdit(this);
    nameLayout->addWidget(m_givenNamesEdit, 1);
    mainLayout->addLayout(nameLayout);

    // Parent/Gender row
    QHBoxLayout* parentGenderLayout = new QHBoxLayout();
    m_parentCheck = new QCheckBox(tr("Parent"), this);
    parentGenderLayout->addWidget(m_parentCheck);
    parentGenderLayout->addSpacing(20);
    parentGenderLayout->addWidget(new QLabel(tr("Gender:"), this));
    m_genderCombo = new QComboBox(this);
    m_genderCombo->addItem(QString(), QString());  // Empty/unspecified
    m_genderCombo->addItem(tr("Male"), QStringLiteral("Male"));
    m_genderCombo->addItem(tr("Female"), QStringLiteral("Female"));
    parentGenderLayout->addWidget(m_genderCombo);
    parentGenderLayout->addStretch();
    mainLayout->addLayout(parentGenderLayout);

    // Birthday section
    QHBoxLayout* birthdayLayout = new QHBoxLayout();
    birthdayLayout->addWidget(new QLabel(tr("Birthday:"), this));
    birthdayLayout->addWidget(new QLabel(tr("Day:"), this));
    m_daySpinner = new QSpinBox(this);
    m_daySpinner->setRange(0, 31);
    m_daySpinner->setSpecialValueText(tr("-"));
    birthdayLayout->addWidget(m_daySpinner);
    birthdayLayout->addWidget(new QLabel(tr("Month:"), this));
    m_monthCombo = new QComboBox(this);
    m_monthCombo->addItem(tr("-"), 0);
    m_monthCombo->addItem(tr("Jan"), 1);
    m_monthCombo->addItem(tr("Feb"), 2);
    m_monthCombo->addItem(tr("Mar"), 3);
    m_monthCombo->addItem(tr("Apr"), 4);
    m_monthCombo->addItem(tr("May"), 5);
    m_monthCombo->addItem(tr("Jun"), 6);
    m_monthCombo->addItem(tr("Jul"), 7);
    m_monthCombo->addItem(tr("Aug"), 8);
    m_monthCombo->addItem(tr("Sep"), 9);
    m_monthCombo->addItem(tr("Oct"), 10);
    m_monthCombo->addItem(tr("Nov"), 11);
    m_monthCombo->addItem(tr("Dec"), 12);
    birthdayLayout->addWidget(m_monthCombo);
    birthdayLayout->addWidget(new QLabel(tr("Year:"), this));
    m_yearEdit = new QLineEdit(this);
    m_yearEdit->setMaximumWidth(60);
    m_yearEdit->setPlaceholderText(tr("(blank=adult)"));
    birthdayLayout->addWidget(m_yearEdit);
    birthdayLayout->addStretch();
    mainLayout->addLayout(birthdayLayout);

    // Contact section
    QFormLayout* contactLayout = new QFormLayout();
    m_phoneEdit = new QLineEdit(this);
    m_phoneEdit->setPlaceholderText(tr("Phone number"));
    contactLayout->addRow(tr("Phone:"), m_phoneEdit);
    m_altPhoneEdit = new QLineEdit(this);
    m_altPhoneEdit->setPlaceholderText(tr("Alternate phone"));
    contactLayout->addRow(tr("Alt:"), m_altPhoneEdit);
    m_emailEdit = new QLineEdit(this);
    m_emailEdit->setPlaceholderText(tr("Email address"));
    contactLayout->addRow(tr("Email:"), m_emailEdit);
    mainLayout->addLayout(contactLayout);

    // Callings section
    mainLayout->addWidget(new QLabel(tr("Callings:"), this));
    m_callingsEditor = new CallingsEditor(this);
    mainLayout->addWidget(m_callingsEditor);

    // Remove button
    QHBoxLayout* removeLayout = new QHBoxLayout();
    removeLayout->addStretch();
    QPushButton* removeBtn = new QPushButton(tr("Remove Member"), this);
    removeBtn->setStyleSheet("color: #c0392b;");
    connect(removeBtn, &QPushButton::clicked, this, &MemberEditor::removeRequested);
    removeLayout->addWidget(removeBtn);
    mainLayout->addLayout(removeLayout);

    // Connect change signals
    connect(m_surnameEdit, &QLineEdit::textChanged, this, &MemberEditor::dataChanged);
    connect(m_givenNamesEdit, &QLineEdit::textChanged, this, &MemberEditor::dataChanged);
    connect(m_parentCheck, &QCheckBox::toggled, this, &MemberEditor::dataChanged);
    connect(m_genderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MemberEditor::dataChanged);
    connect(m_daySpinner, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MemberEditor::dataChanged);
    connect(m_monthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MemberEditor::dataChanged);
    connect(m_yearEdit, &QLineEdit::textChanged, this, &MemberEditor::dataChanged);
    connect(m_phoneEdit, &QLineEdit::textChanged, this, &MemberEditor::dataChanged);
    connect(m_altPhoneEdit, &QLineEdit::textChanged, this, &MemberEditor::dataChanged);
    connect(m_emailEdit, &QLineEdit::textChanged, this, &MemberEditor::dataChanged);
    connect(m_callingsEditor, &CallingsEditor::callingsChanged, this, &MemberEditor::dataChanged);
}

void MemberEditor::setPerson(const Person& person)
{
    m_personId = person.id();

    // Block signals during population
    const QSignalBlocker blocker(this);

    m_surnameEdit->setText(person.surname());
    m_givenNamesEdit->setText(person.givenNames());
    m_parentCheck->setChecked(person.isParent());

    // Gender - use string matching
    if (person.gender().has_value())
    {
        QString genderStr = person.gender()->toString();
        int idx = m_genderCombo->findData(genderStr);
        m_genderCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    else
    {
        m_genderCombo->setCurrentIndex(0);
    }

    // Birthday
    Birthday bday = person.birthday();
    m_daySpinner->setValue(bday.day().value_or(0));
    m_monthCombo->setCurrentIndex(bday.month().value_or(0));
    if (bday.year().has_value())
    {
        m_yearEdit->setText(QString::number(*bday.year()));
    }
    else
    {
        m_yearEdit->clear();
    }

    m_phoneEdit->setText(person.phone());
    m_altPhoneEdit->setText(person.altPhone());
    m_emailEdit->setText(person.email());
    m_callingsEditor->setCallings(person.callings());
}

Person MemberEditor::person() const
{
    // Build birthday
    std::optional<int> year;
    bool ok = false;
    int y = m_yearEdit->text().trimmed().toInt(&ok);
    if (ok && y > 0)
    {
        year = y;
    }

    int month = m_monthCombo->currentData().toInt();
    int day = m_daySpinner->value();

    Birthday birthday = Birthday::create(
        year,
        month > 0 ? std::optional<int>(month) : std::nullopt,
        day > 0 ? std::optional<int>(day) : std::nullopt
    );

    // Build gender from stored string
    std::optional<Gender> gender;
    QString genderStr = m_genderCombo->currentData().toString();
    if (!genderStr.isEmpty())
    {
        gender = Gender::fromString(genderStr);
    }

    // Use createWithId with correct parameter order:
    // id, name, gender, birthday, phone, altPhone, email, callings, isParent
    Person p = Person::createWithId(
        m_personId,
        Name(m_surnameEdit->text().trimmed(), m_givenNamesEdit->text().trimmed()),
        gender,
        birthday,
        Phone(m_phoneEdit->text().trimmed()),
        Phone(m_altPhoneEdit->text().trimmed()),
        m_emailEdit->text().trimmed(),
        m_callingsEditor->callings(),
        m_parentCheck->isChecked()
    );

    return p;
}
