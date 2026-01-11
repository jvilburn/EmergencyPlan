# Family Editing Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add in-place family editing via a side panel in WardListView with visual connection to the selected family.

**Architecture:** Side panel (FamilyEditPanel) attached via QSplitter to WardListView. Edited family row uses sticky positioning with drop shadow when scrolled. Member editing uses inline accordion expansion. All changes buffered locally until explicit Save.

**Tech Stack:** Qt 6 Widgets, C++17, UpdateFamilyCommand for undo/redo

**Design Doc:** [2026-01-11-family-editing-design.md](2026-01-11-family-editing-design.md)

**Working Directory:** `.worktrees/family-editing`

---

## Task Overview

| Task | Component | Description |
|------|-----------|-------------|
| 1 | CallingsEditor | Widget for editing list of callings |
| 2 | MemberEditor | Expanded person fields widget |
| 3 | MemberAccordion | Collapsible container for members |
| 4 | FamilyEditPanel | Main edit panel with address/location/members |
| 5 | WardListView Integration | Add splitter and panel management |
| 6 | Highlight Styling | Shared background color between row and panel |
| 7 | Sticky Row | Pin edited family when scrolled out of view |
| 8 | Unsaved Changes | Dirty detection and save prompt |

---

## Task 1: CallingsEditor Widget

**Files:**
- Create: `src/widgets/CallingsEditor.h`
- Create: `src/widgets/CallingsEditor.cpp`
- Modify: `CMakeLists.txt` (add to EmergencyPlanLib sources)

**Step 1: Create header file**

Create `src/widgets/CallingsEditor.h`:

```cpp
#pragma once

#include <QWidget>
#include <QStringList>
#include <QVBoxLayout>

class QLineEdit;
class QPushButton;

/// Widget for editing a list of callings with add/remove functionality.
class CallingsEditor : public QWidget
{
    Q_OBJECT

public:
    explicit CallingsEditor(QWidget* parent = nullptr);

    void setCallings(const QStringList& callings);
    QStringList callings() const;

signals:
    void callingsChanged();

private slots:
    void onAddCalling();
    void onRemoveCalling();
    void onCallingTextChanged();

private:
    void addCallingRow(const QString& text = QString());
    void rebuildLayout();

    QVBoxLayout* m_layout;
    QList<QLineEdit*> m_callingEdits;
    QPushButton* m_addButton;
};
```

**Step 2: Create implementation file**

Create `src/widgets/CallingsEditor.cpp`:

```cpp
#include "CallingsEditor.h"

#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>

CallingsEditor::CallingsEditor(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);

    m_addButton = new QPushButton(tr("+ Add Calling"), this);
    m_addButton->setFlat(true);
    connect(m_addButton, &QPushButton::clicked, this, &CallingsEditor::onAddCalling);

    m_layout->addWidget(m_addButton);
    m_layout->addStretch();
}

void CallingsEditor::setCallings(const QStringList& callings)
{
    // Clear existing
    for (QLineEdit* edit : m_callingEdits)
    {
        edit->deleteLater();
    }
    m_callingEdits.clear();

    // Add rows for each calling
    for (const QString& calling : callings)
    {
        addCallingRow(calling);
    }
}

QStringList CallingsEditor::callings() const
{
    QStringList result;
    for (QLineEdit* edit : m_callingEdits)
    {
        QString text = edit->text().trimmed();
        if (!text.isEmpty())
        {
            result.append(text);
        }
    }
    return result;
}

void CallingsEditor::onAddCalling()
{
    addCallingRow();
    if (!m_callingEdits.isEmpty())
    {
        m_callingEdits.last()->setFocus();
    }
    emit callingsChanged();
}

void CallingsEditor::onRemoveCalling()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button)
    {
        return;
    }

    // Find the QLineEdit in the same row
    QWidget* row = button->parentWidget();
    QLineEdit* edit = row->findChild<QLineEdit*>();
    if (edit)
    {
        m_callingEdits.removeOne(edit);
    }
    row->deleteLater();
    emit callingsChanged();
}

void CallingsEditor::onCallingTextChanged()
{
    emit callingsChanged();
}

void CallingsEditor::addCallingRow(const QString& text)
{
    QWidget* row = new QWidget(this);
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);

    QLineEdit* edit = new QLineEdit(text, row);
    edit->setPlaceholderText(tr("Calling name"));
    connect(edit, &QLineEdit::textChanged, this, &CallingsEditor::onCallingTextChanged);

    QPushButton* removeBtn = new QPushButton(tr("X"), row);
    removeBtn->setFixedWidth(24);
    removeBtn->setToolTip(tr("Remove calling"));
    connect(removeBtn, &QPushButton::clicked, this, &CallingsEditor::onRemoveCalling);

    rowLayout->addWidget(edit, 1);
    rowLayout->addWidget(removeBtn);

    m_callingEdits.append(edit);

    // Insert before the Add button
    int insertIndex = m_layout->indexOf(m_addButton);
    m_layout->insertWidget(insertIndex, row);
}
```

**Step 3: Add to CMakeLists.txt**

Modify `CMakeLists.txt`, add to `EMERGENCYPLAN_SOURCES`:

```cmake
    src/widgets/CallingsEditor.h
    src/widgets/CallingsEditor.cpp
```

**Step 4: Build and verify**

Run: `./build.bat`
Expected: Build succeeds with 0 errors

**Step 5: Commit**

```bash
git add src/widgets/CallingsEditor.h src/widgets/CallingsEditor.cpp CMakeLists.txt
git commit -m "feat(widgets): add CallingsEditor for editing calling lists

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 2: MemberEditor Widget

**Files:**
- Create: `src/widgets/MemberEditor.h`
- Create: `src/widgets/MemberEditor.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create header file**

Create `src/widgets/MemberEditor.h`:

```cpp
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
    explicit MemberEditor(QWidget* parent = nullptr);

    void setPerson(const Person& person);
    Person person() const;

    /// Returns the original person ID (preserved across edits).
    QString personId() const { return m_personId; }

signals:
    void dataChanged();
    void removeRequested();

private:
    void setupUi();

    QString m_personId;

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
```

**Step 2: Create implementation file**

Create `src/widgets/MemberEditor.cpp`:

```cpp
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
    m_genderCombo->addItem(QString(), QVariant());  // Empty/unspecified
    m_genderCombo->addItem(tr("Male"), QVariant::fromValue(Gender::Male));
    m_genderCombo->addItem(tr("Female"), QVariant::fromValue(Gender::Female));
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

    // Gender
    if (person.gender().has_value())
    {
        int idx = m_genderCombo->findData(QVariant::fromValue(*person.gender()));
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

    // Build gender
    std::optional<Gender> gender;
    QVariant genderData = m_genderCombo->currentData();
    if (genderData.isValid() && !genderData.isNull())
    {
        gender = genderData.value<Gender>();
    }

    Person p = Person::createWithId(
        m_personId,
        Name(m_surnameEdit->text().trimmed(), m_givenNamesEdit->text().trimmed()),
        m_parentCheck->isChecked(),
        Phone(m_phoneEdit->text().trimmed()),
        Phone(m_altPhoneEdit->text().trimmed()),
        Email(m_emailEdit->text().trimmed()),
        gender,
        birthday
    );
    p.setCallings(m_callingsEditor->callings());

    return p;
}
```

**Step 3: Add to CMakeLists.txt**

Add to `EMERGENCYPLAN_SOURCES`:

```cmake
    src/widgets/MemberEditor.h
    src/widgets/MemberEditor.cpp
```

**Step 4: Build and verify**

Run: `./build.bat`
Expected: Build succeeds with 0 errors

**Step 5: Commit**

```bash
git add src/widgets/MemberEditor.h src/widgets/MemberEditor.cpp CMakeLists.txt
git commit -m "feat(widgets): add MemberEditor for editing person fields

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 3: MemberAccordion Widget

**Files:**
- Create: `src/widgets/MemberAccordion.h`
- Create: `src/widgets/MemberAccordion.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create header file**

Create `src/widgets/MemberAccordion.h`:

```cpp
#pragma once

#include <QWidget>
#include <QList>
#include "Person.h"

class QVBoxLayout;
class QScrollArea;
class MemberEditor;
class QPushButton;

/// Accordion widget showing family members with expandable editors.
class MemberAccordion : public QWidget
{
    Q_OBJECT

public:
    explicit MemberAccordion(QWidget* parent = nullptr);

    void setMembers(const QList<Person>& members);
    QList<Person> members() const;

signals:
    void membersChanged();

private slots:
    void onAddMember();
    void onRemoveMember();
    void onMemberDataChanged();

private:
    struct MemberRow
    {
        QWidget* header;
        MemberEditor* editor;
        Person originalPerson;
        bool expanded;
    };

    void addMemberRow(const Person& person, bool expanded = false);
    void toggleRow(int index);
    void updateHeaderText(int index);
    void rebuildLayout();

    QVBoxLayout* m_contentLayout;
    QList<MemberRow> m_rows;
    QPushButton* m_addButton;
};
```

**Step 2: Create implementation file**

Create `src/widgets/MemberAccordion.cpp`:

```cpp
#include "MemberAccordion.h"
#include "MemberEditor.h"
#include "Gender.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>

MemberAccordion::MemberAccordion(QWidget* parent)
    : QWidget(parent)
    , m_contentLayout(new QVBoxLayout(this))
{
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(4);

    // Add member button
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->addWidget(new QLabel(tr("Members"), this));
    headerLayout->addStretch();
    m_addButton = new QPushButton(tr("+ Add"), this);
    m_addButton->setFlat(true);
    connect(m_addButton, &QPushButton::clicked, this, &MemberAccordion::onAddMember);
    headerLayout->addWidget(m_addButton);
    m_contentLayout->addLayout(headerLayout);

    m_contentLayout->addStretch();
}

void MemberAccordion::setMembers(const QList<Person>& members)
{
    // Clear existing rows
    for (const MemberRow& row : m_rows)
    {
        row.header->deleteLater();
        row.editor->deleteLater();
    }
    m_rows.clear();

    // Add rows for each member
    for (const Person& person : members)
    {
        addMemberRow(person, false);
    }
}

QList<Person> MemberAccordion::members() const
{
    QList<Person> result;
    for (const MemberRow& row : m_rows)
    {
        result.append(row.editor->person());
    }
    return result;
}

void MemberAccordion::onAddMember()
{
    Person newPerson = Person::create(
        Name(QString(), QString()),
        false,  // isParent
        Phone(),
        Phone(),
        Email(),
        std::nullopt,
        Birthday()
    );
    addMemberRow(newPerson, true);
    emit membersChanged();
}

void MemberAccordion::onRemoveMember()
{
    MemberEditor* editor = qobject_cast<MemberEditor*>(sender());
    if (!editor)
    {
        return;
    }

    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].editor == editor)
        {
            m_rows[i].header->deleteLater();
            m_rows[i].editor->deleteLater();
            m_rows.removeAt(i);
            emit membersChanged();
            return;
        }
    }
}

void MemberAccordion::onMemberDataChanged()
{
    // Update header text for the changed member
    MemberEditor* editor = qobject_cast<MemberEditor*>(sender());
    if (!editor)
    {
        return;
    }

    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].editor == editor)
        {
            updateHeaderText(i);
            break;
        }
    }
    emit membersChanged();
}

void MemberAccordion::addMemberRow(const Person& person, bool expanded)
{
    MemberRow row;
    row.originalPerson = person;
    row.expanded = expanded;

    // Create header (clickable bar)
    row.header = new QFrame(this);
    row.header->setFrameShape(QFrame::StyledPanel);
    row.header->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* headerLayout = new QHBoxLayout(row.header);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    // Parent indicator and name
    QString indicator = person.isParent() ? QString::fromUtf8("\u25CF") : QString::fromUtf8("\u25CB");
    QLabel* nameLabel = new QLabel(row.header);
    nameLabel->setObjectName("nameLabel");
    headerLayout->addWidget(nameLabel);
    headerLayout->addStretch();

    // Expand indicator
    QLabel* expandLabel = new QLabel(expanded ? QString::fromUtf8("\u25BC") : QString::fromUtf8("\u25B6"), row.header);
    expandLabel->setObjectName("expandLabel");
    headerLayout->addWidget(expandLabel);

    // Create editor
    row.editor = new MemberEditor(this);
    row.editor->setPerson(person);
    row.editor->setVisible(expanded);
    connect(row.editor, &MemberEditor::dataChanged, this, &MemberAccordion::onMemberDataChanged);
    connect(row.editor, &MemberEditor::removeRequested, this, &MemberAccordion::onRemoveMember);

    // Click handler for header
    int rowIndex = m_rows.size();
    row.header->installEventFilter(this);
    row.header->setProperty("rowIndex", rowIndex);

    m_rows.append(row);
    updateHeaderText(rowIndex);

    // Insert before stretch
    int insertPos = m_contentLayout->count() - 1;  // Before stretch
    m_contentLayout->insertWidget(insertPos, row.header);
    m_contentLayout->insertWidget(insertPos + 1, row.editor);
}

void MemberAccordion::toggleRow(int index)
{
    if (index < 0 || index >= m_rows.size())
    {
        return;
    }

    MemberRow& row = m_rows[index];
    row.expanded = !row.expanded;
    row.editor->setVisible(row.expanded);

    // Update expand indicator
    QLabel* expandLabel = row.header->findChild<QLabel*>("expandLabel");
    if (expandLabel)
    {
        expandLabel->setText(row.expanded ? QString::fromUtf8("\u25BC") : QString::fromUtf8("\u25B6"));
    }
}

void MemberAccordion::updateHeaderText(int index)
{
    if (index < 0 || index >= m_rows.size())
    {
        return;
    }

    const MemberRow& row = m_rows[index];
    Person p = row.editor->person();

    QString indicator = p.isParent() ? QString::fromUtf8("\u25CF ") : QString::fromUtf8("\u25CB ");
    QString name = p.displayName();
    if (name.isEmpty())
    {
        name = tr("(New Member)");
    }

    QString details;
    if (p.gender().has_value())
    {
        details += Gender::toString(*p.gender()).left(1);
    }
    if (!p.birthday().dateDisplay().isEmpty())
    {
        if (!details.isEmpty()) details += QString::fromUtf8(" \u00B7 ");
        details += p.birthday().dateDisplay();
        if (!p.birthday().ageDisplay().isEmpty())
        {
            details += " (" + p.birthday().ageDisplay() + ")";
        }
    }
    if (!p.displayPhone().isEmpty())
    {
        if (!details.isEmpty()) details += QString::fromUtf8(" \u00B7 ");
        details += p.displayPhone();
    }

    QString text = indicator + name;
    if (!details.isEmpty())
    {
        text += "\n   " + details;
    }

    QLabel* nameLabel = row.header->findChild<QLabel*>("nameLabel");
    if (nameLabel)
    {
        nameLabel->setText(text);
    }
}

void MemberAccordion::rebuildLayout()
{
    // Remove all widgets
    while (m_contentLayout->count() > 0)
    {
        QLayoutItem* item = m_contentLayout->takeAt(0);
        delete item;
    }

    // Re-add header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->addWidget(new QLabel(tr("Members"), this));
    headerLayout->addStretch();
    headerLayout->addWidget(m_addButton);
    m_contentLayout->addLayout(headerLayout);

    // Re-add all rows
    for (const MemberRow& row : m_rows)
    {
        m_contentLayout->addWidget(row.header);
        m_contentLayout->addWidget(row.editor);
    }

    m_contentLayout->addStretch();
}
```

**Step 3: Add event filter for header clicks**

Add to `MemberAccordion.cpp` before the closing brace of the class:

```cpp
bool MemberAccordion::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease)
    {
        QWidget* header = qobject_cast<QWidget*>(obj);
        if (header)
        {
            int index = header->property("rowIndex").toInt();
            toggleRow(index);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
```

Add to header file, under `private:`:

```cpp
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
```

And add include at top of .cpp:

```cpp
#include <QEvent>
#include <QMouseEvent>
```

**Step 4: Add to CMakeLists.txt**

Add to `EMERGENCYPLAN_SOURCES`:

```cmake
    src/widgets/MemberAccordion.h
    src/widgets/MemberAccordion.cpp
```

**Step 5: Build and verify**

Run: `./build.bat`
Expected: Build succeeds with 0 errors

**Step 6: Commit**

```bash
git add src/widgets/MemberAccordion.h src/widgets/MemberAccordion.cpp CMakeLists.txt
git commit -m "feat(widgets): add MemberAccordion for collapsible member editing

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 4: FamilyEditPanel Widget

**Files:**
- Create: `src/widgets/FamilyEditPanel.h`
- Create: `src/widgets/FamilyEditPanel.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create header file**

Create `src/widgets/FamilyEditPanel.h`:

```cpp
#pragma once

#include <QFrame>
#include "Family.h"

class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class MemberAccordion;
class DocumentManager;

/// Side panel for editing a family's address, location, and members.
class FamilyEditPanel : public QFrame
{
    Q_OBJECT

public:
    explicit FamilyEditPanel(DocumentManager* docManager, QWidget* parent = nullptr);

    void setFamily(const Family& family);
    Family family() const;

    QString familyId() const { return m_familyId; }
    bool isDirty() const;

signals:
    void saveRequested();
    void cancelRequested();
    void closeRequested();
    void dataChanged();

private slots:
    void onLookUpCoordinates();
    void onCoordinatesReceived(double lat, double lon);
    void onGeocodingFailed(const QString& error);

private:
    void setupUi();
    void updateTitle();

    DocumentManager* m_docManager;
    QString m_familyId;
    Family m_originalFamily;

    QLabel* m_titleLabel;
    QPushButton* m_closeButton;

    // Address
    QTextEdit* m_addressEdit;

    // Location
    QLineEdit* m_latEdit;
    QLineEdit* m_lonEdit;
    QPushButton* m_lookupButton;

    // Members
    MemberAccordion* m_memberAccordion;

    // Actions
    QPushButton* m_cancelButton;
    QPushButton* m_saveButton;
};
```

**Step 2: Create implementation file**

Create `src/widgets/FamilyEditPanel.cpp`:

```cpp
#include "FamilyEditPanel.h"
#include "MemberAccordion.h"
#include "DocumentManager.h"
#include "GeocodingService.h"
#include "Address.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QMessageBox>

FamilyEditPanel::FamilyEditPanel(DocumentManager* docManager, QWidget* parent)
    : QFrame(parent)
    , m_docManager(docManager)
{
    setupUi();
}

void FamilyEditPanel::setupUi()
{
    setFrameShape(QFrame::StyledPanel);
    setMinimumWidth(300);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 12);
    mainLayout->setSpacing(12);

    // Title bar with close button
    QHBoxLayout* titleLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(tr("Edit Family"), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    m_closeButton = new QPushButton(tr("X"), this);
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setToolTip(tr("Close"));
    connect(m_closeButton, &QPushButton::clicked, this, &FamilyEditPanel::closeRequested);
    titleLayout->addWidget(m_closeButton);
    mainLayout->addLayout(titleLayout);

    // Scroll area for content
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* scrollContent = new QWidget(scrollArea);
    QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(0, 0, 8, 0);  // Right margin for scrollbar
    contentLayout->setSpacing(12);

    // Address section
    contentLayout->addWidget(new QLabel(tr("Address"), this));
    m_addressEdit = new QTextEdit(this);
    m_addressEdit->setMaximumHeight(80);
    m_addressEdit->setPlaceholderText(tr("Enter address (one line per row)"));
    connect(m_addressEdit, &QTextEdit::textChanged, this, &FamilyEditPanel::dataChanged);
    contentLayout->addWidget(m_addressEdit);

    // Location section
    QHBoxLayout* locationHeaderLayout = new QHBoxLayout();
    locationHeaderLayout->addWidget(new QLabel(tr("Location"), this));
    locationHeaderLayout->addStretch();
    m_lookupButton = new QPushButton(tr("Look Up Coordinates"), this);
    connect(m_lookupButton, &QPushButton::clicked, this, &FamilyEditPanel::onLookUpCoordinates);
    locationHeaderLayout->addWidget(m_lookupButton);
    contentLayout->addLayout(locationHeaderLayout);

    QHBoxLayout* coordLayout = new QHBoxLayout();
    coordLayout->addWidget(new QLabel(tr("Lat:"), this));
    m_latEdit = new QLineEdit(this);
    m_latEdit->setPlaceholderText(tr("Latitude"));
    connect(m_latEdit, &QLineEdit::textChanged, this, &FamilyEditPanel::dataChanged);
    coordLayout->addWidget(m_latEdit);
    coordLayout->addWidget(new QLabel(tr("Lon:"), this));
    m_lonEdit = new QLineEdit(this);
    m_lonEdit->setPlaceholderText(tr("Longitude"));
    connect(m_lonEdit, &QLineEdit::textChanged, this, &FamilyEditPanel::dataChanged);
    coordLayout->addWidget(m_lonEdit);
    contentLayout->addLayout(coordLayout);

    // Separator
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    contentLayout->addWidget(separator);

    // Members section
    m_memberAccordion = new MemberAccordion(this);
    connect(m_memberAccordion, &MemberAccordion::membersChanged, this, &FamilyEditPanel::dataChanged);
    contentLayout->addWidget(m_memberAccordion);

    contentLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &FamilyEditPanel::cancelRequested);
    buttonLayout->addWidget(m_cancelButton);
    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setDefault(true);
    connect(m_saveButton, &QPushButton::clicked, this, &FamilyEditPanel::saveRequested);
    buttonLayout->addWidget(m_saveButton);
    mainLayout->addLayout(buttonLayout);
}

void FamilyEditPanel::setFamily(const Family& family)
{
    m_familyId = family.id();
    m_originalFamily = family;

    // Block signals during population
    const QSignalBlocker blocker(this);

    // Address
    m_addressEdit->setPlainText(family.address().multiLine());

    // Location
    if (family.latitude().has_value())
    {
        m_latEdit->setText(QString::number(*family.latitude(), 'f', 6));
    }
    else
    {
        m_latEdit->clear();
    }

    if (family.longitude().has_value())
    {
        m_lonEdit->setText(QString::number(*family.longitude(), 'f', 6));
    }
    else
    {
        m_lonEdit->clear();
    }

    // Members
    m_memberAccordion->setMembers(family.members());

    updateTitle();
}

Family FamilyEditPanel::family() const
{
    // Build address from text
    Address address;
    QStringList lines = m_addressEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines)
    {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
        {
            address.addLine(trimmed);
        }
    }

    // Parse coordinates
    std::optional<double> lat;
    std::optional<double> lon;
    bool latOk = false, lonOk = false;
    double latVal = m_latEdit->text().trimmed().toDouble(&latOk);
    double lonVal = m_lonEdit->text().trimmed().toDouble(&lonOk);
    if (latOk)
    {
        lat = latVal;
    }
    if (lonOk)
    {
        lon = lonVal;
    }

    Family f = Family::createWithId(
        m_familyId,
        lat,
        lon,
        address,
        m_memberAccordion->members()
    );

    return f;
}

bool FamilyEditPanel::isDirty() const
{
    return family() != m_originalFamily;
}

void FamilyEditPanel::onLookUpCoordinates()
{
    QString addressText = m_addressEdit->toPlainText().trimmed();
    if (addressText.isEmpty())
    {
        QMessageBox::warning(this, tr("No Address"), tr("Please enter an address first."));
        return;
    }

    m_lookupButton->setEnabled(false);
    m_lookupButton->setText(tr("Looking up..."));

    GeocodingService* geocoder = new GeocodingService(this);
    connect(geocoder, &GeocodingService::coordinatesReceived,
            this, &FamilyEditPanel::onCoordinatesReceived);
    connect(geocoder, &GeocodingService::geocodingFailed,
            this, &FamilyEditPanel::onGeocodingFailed);
    connect(geocoder, &GeocodingService::coordinatesReceived,
            geocoder, &QObject::deleteLater);
    connect(geocoder, &GeocodingService::geocodingFailed,
            geocoder, &QObject::deleteLater);

    // Join address lines with commas for geocoding
    QString query = addressText.replace('\n', ", ");
    geocoder->geocode(query);
}

void FamilyEditPanel::onCoordinatesReceived(double lat, double lon)
{
    m_latEdit->setText(QString::number(lat, 'f', 6));
    m_lonEdit->setText(QString::number(lon, 'f', 6));

    m_lookupButton->setEnabled(true);
    m_lookupButton->setText(tr("Look Up Coordinates"));
}

void FamilyEditPanel::onGeocodingFailed(const QString& error)
{
    m_lookupButton->setEnabled(true);
    m_lookupButton->setText(tr("Look Up Coordinates"));

    QMessageBox::warning(this, tr("Lookup Failed"),
        tr("Could not find coordinates for this address.\n\n%1").arg(error));
}

void FamilyEditPanel::updateTitle()
{
    QString surname = m_originalFamily.surname();
    if (surname.isEmpty())
    {
        surname = tr("Family");
    }
    m_titleLabel->setText(tr("Edit: %1").arg(surname));
}
```

**Step 3: Add to CMakeLists.txt**

Add to `EMERGENCYPLAN_SOURCES`:

```cmake
    src/widgets/FamilyEditPanel.h
    src/widgets/FamilyEditPanel.cpp
```

**Step 4: Build and verify**

Run: `./build.bat`
Expected: Build succeeds with 0 errors

**Step 5: Commit**

```bash
git add src/widgets/FamilyEditPanel.h src/widgets/FamilyEditPanel.cpp CMakeLists.txt
git commit -m "feat(widgets): add FamilyEditPanel for editing family data

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 5: WardListView Integration

**Files:**
- Modify: `src/widgets/WardListView.h`
- Modify: `src/widgets/WardListView.cpp`

**Step 1: Add panel and splitter to WardListView header**

Modify `src/widgets/WardListView.h`:

Add forward declaration near top:
```cpp
class FamilyEditPanel;
class QSplitter;
```

Add to public section:
```cpp
    /// Returns true if an edit panel is currently open.
    bool isEditing() const;

    /// Close the edit panel if open (prompts for unsaved changes).
    /// Returns false if user cancelled.
    bool closeEditPanel();
```

Add to private slots:
```cpp
    void onEditFamily(const QString& familyId);
    void onSaveFamily();
    void onCancelEdit();
    void onCloseEditPanel();
```

Add to private members:
```cpp
    QSplitter* m_splitter;
    FamilyEditPanel* m_editPanel;
    QString m_editingFamilyId;
```

**Step 2: Implement panel management in WardListView**

Modify `src/widgets/WardListView.cpp`:

Add includes:
```cpp
#include "FamilyEditPanel.h"
#include "FamilyCommands.h"
#include <QSplitter>
#include <QMessageBox>
```

In constructor, wrap existing layout in splitter:

```cpp
// Replace direct layout with splitter
m_splitter = new QSplitter(Qt::Horizontal, this);

// Create container for existing content
QWidget* listContainer = new QWidget(m_splitter);
QVBoxLayout* listLayout = new QVBoxLayout(listContainer);
listLayout->setContentsMargins(0, 0, 0, 0);
listLayout->addWidget(m_searchField);
listLayout->addWidget(m_treeView, 1);

m_splitter->addWidget(listContainer);

// Edit panel (hidden by default)
m_editPanel = new FamilyEditPanel(m_documentManager, m_splitter);
m_editPanel->hide();
m_splitter->addWidget(m_editPanel);

// Set splitter as main widget
QVBoxLayout* mainLayout = new QVBoxLayout(this);
mainLayout->setContentsMargins(0, 0, 0, 0);
mainLayout->addWidget(m_splitter);

// Connect edit panel signals
connect(m_editPanel, &FamilyEditPanel::saveRequested, this, &WardListView::onSaveFamily);
connect(m_editPanel, &FamilyEditPanel::cancelRequested, this, &WardListView::onCancelEdit);
connect(m_editPanel, &FamilyEditPanel::closeRequested, this, &WardListView::onCloseEditPanel);
```

Connect the editFamilyRequested signal:
```cpp
connect(this, &WardListView::editFamilyRequested, this, &WardListView::onEditFamily);
```

Implement the slots:

```cpp
void WardListView::onEditFamily(const QString& familyId)
{
    // Close existing panel if editing different family
    if (isEditing() && m_editingFamilyId != familyId)
    {
        if (!closeEditPanel())
        {
            return;  // User cancelled
        }
    }

    // Get family from document
    const Document& doc = m_documentManager->document();
    if (!doc.families().contains(familyId))
    {
        return;
    }

    const Family& family = doc.families().value(familyId);
    m_editingFamilyId = familyId;
    m_editPanel->setFamily(family);
    m_editPanel->show();

    // Set splitter sizes (list:panel = 1:1)
    m_splitter->setSizes({m_splitter->width() / 2, m_splitter->width() / 2});
}

void WardListView::onSaveFamily()
{
    if (!isEditing())
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    Family oldFamily = doc.families().value(m_editingFamilyId);
    Family newFamily = m_editPanel->family();

    m_documentManager->executeCommand(
        std::make_unique<UpdateFamilyCommand>(oldFamily, newFamily));

    m_editingFamilyId.clear();
    m_editPanel->hide();
}

void WardListView::onCancelEdit()
{
    m_editingFamilyId.clear();
    m_editPanel->hide();
}

void WardListView::onCloseEditPanel()
{
    closeEditPanel();
}

bool WardListView::isEditing() const
{
    return !m_editingFamilyId.isEmpty() && m_editPanel->isVisible();
}

bool WardListView::closeEditPanel()
{
    if (!isEditing())
    {
        return true;
    }

    if (m_editPanel->isDirty())
    {
        const Family& original = m_documentManager->document().families().value(m_editingFamilyId);
        QString familyName = original.surname();
        if (familyName.isEmpty())
        {
            familyName = tr("this family");
        }

        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Save changes to %1?").arg(familyName),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (result == QMessageBox::Save)
        {
            onSaveFamily();
            return true;
        }
        else if (result == QMessageBox::Cancel)
        {
            return false;
        }
        // Discard - fall through
    }

    m_editingFamilyId.clear();
    m_editPanel->hide();
    return true;
}
```

**Step 3: Build and verify**

Run: `./build.bat`
Expected: Build succeeds with 0 errors

**Step 4: Manual test**

1. Run the application
2. Open a ward file
3. Expand a family and click Edit
4. Verify panel appears on the right
5. Make a change and click Save
6. Verify change persists
7. Click Edit, make change, click Cancel
8. Verify change is discarded

**Step 5: Commit**

```bash
git add src/widgets/WardListView.h src/widgets/WardListView.cpp
git commit -m "feat(WardListView): integrate FamilyEditPanel with splitter

- Panel shows on Edit click
- Save executes UpdateFamilyCommand
- Cancel/Close with unsaved changes prompts user

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 6: Highlight Styling

**Files:**
- Modify: `src/widgets/WardListView.h`
- Modify: `src/widgets/WardListView.cpp`
- Modify: `src/widgets/FamilyEditPanel.cpp`

**Step 1: Define shared highlight color**

Add to `WardListView.h` or create a shared constants header:

```cpp
namespace EditHighlight
{
    const QString BackgroundColor = "#e3f2fd";  // Light blue
    const QString BorderColor = "#90caf9";
}
```

**Step 2: Apply highlight to edit panel**

In `FamilyEditPanel.cpp`, in `setupUi()` after setting frame shape:

```cpp
setStyleSheet(QString(
    "FamilyEditPanel { "
    "  background-color: %1; "
    "  border-left: 3px solid %2; "
    "}"
).arg(EditHighlight::BackgroundColor, EditHighlight::BorderColor));
```

**Step 3: Apply highlight to tree row when editing**

In `WardListView.cpp`, add method to highlight the edited row:

```cpp
void WardListView::updateEditHighlight()
{
    // This requires custom delegate painting - simplified version uses selection
    if (!m_editingFamilyId.isEmpty())
    {
        QModelIndex index = m_model->indexForFamilyId(m_editingFamilyId);
        if (index.isValid())
        {
            m_treeView->setCurrentIndex(index);
            m_treeView->scrollTo(index);
        }
    }
}
```

Call `updateEditHighlight()` in `onEditFamily()` after showing the panel.

**Step 4: Build and verify**

Run: `./build.bat`
Expected: Build succeeds, panel has light blue background

**Step 5: Commit**

```bash
git add src/widgets/WardListView.h src/widgets/WardListView.cpp src/widgets/FamilyEditPanel.cpp
git commit -m "style: add shared highlight color for edit panel and row

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Task 7: Sticky Row (Deferred)

**Note:** Sticky row positioning requires custom overlay painting on the tree view viewport. This is medium complexity and can be implemented as a follow-up enhancement.

**Files to modify when implementing:**
- `src/widgets/WardListView.h` - add overlay widget
- `src/widgets/WardListView.cpp` - viewport event filter, scroll handling, overlay painting

**Key implementation points:**
1. Install event filter on tree view viewport
2. Track scroll position to detect when edited row leaves viewport
3. Create overlay QWidget that paints the row with drop shadow
4. Position overlay at top or bottom of viewport as needed

**Skip for initial implementation - create follow-up issue.**

---

## Task 8: Final Cleanup and Testing

**Step 1: Initialize edit panel member**

Ensure `m_editPanel` and `m_editingFamilyId` are initialized in constructor:

```cpp
m_editPanel = nullptr;
m_editingFamilyId = QString();
```

**Step 2: Full manual testing**

Test checklist:
- [ ] Click Edit on family → panel opens
- [ ] Edit address → Save → address persists
- [ ] Edit member name → Save → name persists
- [ ] Add new member → Save → member appears
- [ ] Remove member → Save → member removed
- [ ] Edit, then Cancel → changes discarded
- [ ] Edit, close with X → prompted to save
- [ ] Edit family A, click Edit on family B → prompted about A
- [ ] Undo after save → changes reverted
- [ ] Look Up Coordinates → coordinates populated

**Step 3: Commit any fixes**

```bash
git add -A
git commit -m "fix: address issues found in manual testing

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Summary

| Task | Component | Status |
|------|-----------|--------|
| 1 | CallingsEditor | Ready |
| 2 | MemberEditor | Ready |
| 3 | MemberAccordion | Ready |
| 4 | FamilyEditPanel | Ready |
| 5 | WardListView Integration | Ready |
| 6 | Highlight Styling | Ready |
| 7 | Sticky Row | Deferred |
| 8 | Testing & Cleanup | Ready |

**Total estimated tasks:** 7 (excluding deferred sticky row)

**After completion:** Use `superpowers:finishing-a-development-branch` to merge or create PR.
