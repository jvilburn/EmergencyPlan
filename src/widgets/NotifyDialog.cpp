#include "NotifyDialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>

NotifyDialog::NotifyDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Notify Assignee"));
    setMinimumWidth(350);

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

    // Notes field
    QLabel* notesLabel = new QLabel(tr("Notes:"), this);
    layout->addWidget(notesLabel);

    m_notesEdit = new QLineEdit(this);
    m_notesEdit->setPlaceholderText(tr("Optional"));
    layout->addWidget(m_notesEdit);

    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(this);
    QPushButton* notifyBtn = buttons->addButton(tr("Notify"), QDialogButtonBox::AcceptRole);
    Q_UNUSED(notifyBtn)
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted,
            this, &NotifyDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

std::optional<TaskNotification> NotifyDialog::result() const
{
    return m_result;
}

void NotifyDialog::onAccepted()
{
    ContactMethod method = static_cast<ContactMethod>(m_methodGroup->checkedId());
    QString notes = m_notesEdit->text().trimmed();
    m_result = TaskNotification::create(method, notes);
    accept();
}
