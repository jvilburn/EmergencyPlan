#include "ContactAttemptDialog.h"
#include "DocumentManager.h"
#include "PersonCombo.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRadioButton>
#include <QVBoxLayout>

ContactAttemptDialog::ContactAttemptDialog(const FamilyId& familyId, QWidget* parent)
    : QDialog(parent)
    , m_familyId(familyId)
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

    // Who: blank records no one
    QLabel* whoLabel = new QLabel(tr("Who:"), this);
    layout->addWidget(whoLabel);

    m_whoCombo = new QComboBox(this);
    PersonCombo::fill(m_whoCombo, DocumentManager::instance()->document(), m_familyId);
    layout->addWidget(m_whoCombo);

    // Outcome: none, or the status the contact found
    QLabel* outcomeLabel = new QLabel(tr("Outcome:"), this);
    layout->addWidget(outcomeLabel);

    QHBoxLayout* outcomeRow = new QHBoxLayout();
    QRadioButton* noOutcomeRadio = new QRadioButton(tr("None"), this);
    m_okRadio = new QRadioButton(tr("OK"), this);
    m_unableToReachRadio = new QRadioButton(tr("Unable to Reach"), this);
    outcomeRow->addWidget(noOutcomeRadio);
    outcomeRow->addWidget(m_okRadio);
    outcomeRow->addWidget(m_unableToReachRadio);
    outcomeRow->addStretch();
    layout->addLayout(outcomeRow);

    QButtonGroup* outcomeGroup = new QButtonGroup(this);
    outcomeGroup->addButton(noOutcomeRadio);
    outcomeGroup->addButton(m_okRadio);
    outcomeGroup->addButton(m_unableToReachRadio);
    noOutcomeRadio->setChecked(true);

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

    std::optional<ContactOutcome> outcome;
    if (m_okRadio->isChecked())
    {
        outcome = ContactOutcome::OK;
    }
    else if (m_unableToReachRadio->isChecked())
    {
        outcome = ContactOutcome::UnableToReach;
    }

    m_result = ContactAttempt::create(m_familyId, method, PersonCombo::chosen(m_whoCombo), outcome, notes);
    accept();
}
