#include "ContactAttemptDialog.h"
#include "Document.h"
#include "DocumentManager.h"
#include "Family.h"
#include "Person.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRadioButton>
#include <QVBoxLayout>

ContactAttemptDialog::ContactAttemptDialog(DocumentManager* documentManager,
                                           QWidget* parent)
    : QDialog(parent)
    , m_documentManager(documentManager)
{
    setWindowTitle(tr("Log Contact Attempt"));
    setMinimumWidth(400);

    QVBoxLayout* layout = new QVBoxLayout(this);

    // Method radio buttons
    QLabel* methodLabel = new QLabel(tr("Method:"), this);
    layout->addWidget(methodLabel);

    m_methodGroup = new QButtonGroup(this);

    QHBoxLayout* methodRow1 = new QHBoxLayout();
    QRadioButton* phoneRadio = new QRadioButton(tr("Phone"), this);
    QRadioButton* textRadio = new QRadioButton(tr("Text"), this);
    QRadioButton* emailRadio = new QRadioButton(tr("Email"), this);
    methodRow1->addWidget(phoneRadio);
    methodRow1->addWidget(textRadio);
    methodRow1->addWidget(emailRadio);
    layout->addLayout(methodRow1);

    QHBoxLayout* methodRow2 = new QHBoxLayout();
    QRadioButton* visitRadio = new QRadioButton(tr("Visit"), this);
    QRadioButton* otherRadio = new QRadioButton(tr("Other"), this);
    methodRow2->addWidget(visitRadio);
    methodRow2->addWidget(otherRadio);
    methodRow2->addStretch();
    layout->addLayout(methodRow2);

    m_methodGroup->addButton(phoneRadio, static_cast<int>(ContactMethod::Phone));
    m_methodGroup->addButton(textRadio, static_cast<int>(ContactMethod::Text));
    m_methodGroup->addButton(emailRadio, static_cast<int>(ContactMethod::Email));
    m_methodGroup->addButton(visitRadio, static_cast<int>(ContactMethod::Visit));
    m_methodGroup->addButton(otherRadio, static_cast<int>(ContactMethod::Other));
    phoneRadio->setChecked(true);

    // Who combo box
    QLabel* whoLabel = new QLabel(tr("Who:"), this);
    layout->addWidget(whoLabel);

    m_whoCombo = new QComboBox(this);
    m_whoCombo->setEditable(true);
    m_whoCombo->setInsertPolicy(QComboBox::NoInsert);

    // Populate with all persons from document, sorted by display name
    struct PersonEntry
    {
        QString displayName;
        PersonId id;
    };
    QList<PersonEntry> entries;

    const QHash<FamilyId, Family>& families = m_documentManager->document().families();
    for (auto it = families.constBegin(); it != families.constEnd(); ++it)
    {
        for (const Person& person : it.value().members())
        {
            entries.append({person.displayName(), person.id()});
        }
    }

    std::sort(entries.begin(), entries.end(),
              [](const PersonEntry& a, const PersonEntry& b) { return a.displayName.toLower() < b.displayName.toLower(); });

    for (const PersonEntry& entry : entries)
    {
        m_whoCombo->addItem(entry.displayName, entry.id.toString());
    }

    layout->addWidget(m_whoCombo);

    // Notes field
    QLabel* notesLabel = new QLabel(tr("Notes:"), this);
    layout->addWidget(notesLabel);

    m_notesEdit = new QLineEdit(this);
    m_notesEdit->setPlaceholderText(tr("Optional (required for Other method)"));
    layout->addWidget(m_notesEdit);

    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted,
            this, &ContactAttemptDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

std::optional<ContactAttempt> ContactAttemptDialog::result() const
{
    return m_result;
}

void ContactAttemptDialog::onAccepted()
{
    // Validate selection
    int comboIndex = m_whoCombo->currentIndex();
    if (comboIndex < 0)
    {
        QMessageBox::warning(this, tr("Log Contact Attempt"),
                             tr("Please select a person."));
        return;
    }

    ContactMethod method = static_cast<ContactMethod>(m_methodGroup->checkedId());
    QString notes = m_notesEdit->text().trimmed();

    // Notes required for "Other" method
    if (method == ContactMethod::Other && notes.isEmpty())
    {
        QMessageBox::warning(this, tr("Log Contact Attempt"),
                             tr("Notes are required when method is Other."));
        m_notesEdit->setFocus();
        return;
    }

    PersonId who = PersonId::fromString(m_whoCombo->currentData().toString());
    m_result = ContactAttempt::create(method, who, notes);

    accept();
}
