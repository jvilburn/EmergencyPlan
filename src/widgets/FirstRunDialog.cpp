#include "FirstRunDialog.h"

#include "IdentityService.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

FirstRunDialog::FirstRunDialog(IdentityService& identity, QWidget* parent)
    : QDialog(parent)
    , m_identity(identity)
{
    setWindowTitle(tr("Welcome to Ward Planning"));
    setMinimumWidth(460);

    QFormLayout* form = new QFormLayout(this);
    QLabel* why = new QLabel(tr("Others with access to your wards see these names. If you already use Ward Planning "
                                "on another device, choose Add a Device there and open the link it gives you here."),
                             this);
    why->setWordWrap(true);
    form->addRow(why);
    m_displayName = new QLineEdit(this);
    m_displayName->setPlaceholderText(tr("Your name, as your ward knows you"));
    form->addRow(tr("Your name:"), m_displayName);
    m_deviceName = new QLineEdit(IdentityService::defaultDeviceName(), this);
    form->addRow(tr("This device:"), m_deviceName);

    QDialogButtonBox* buttons = new QDialogButtonBox(this);
    m_createButton = buttons->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    form->addRow(buttons);

    connect(m_createButton, &QPushButton::clicked, this, &FirstRunDialog::onCreate);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_displayName, &QLineEdit::textChanged, this, &FirstRunDialog::onNamesChanged);
    connect(m_deviceName, &QLineEdit::textChanged, this, &FirstRunDialog::onNamesChanged);
    onNamesChanged();
    m_displayName->setFocus();
}

void FirstRunDialog::onNamesChanged()
{
    m_createButton->setEnabled(!m_displayName->text().trimmed().isEmpty()
                               && !m_deviceName->text().trimmed().isEmpty());
}

void FirstRunDialog::onCreate()
{
    QString error;
    if (!m_identity.createPerson(m_displayName->text(), m_deviceName->text(), &error))
    {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    accept();
}
