#include "FirstRunDialog.h"

#include "IdentityService.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

FirstRunDialog::FirstRunDialog(IdentityService& identity, QWidget* parent)
    : QDialog(parent)
    , m_identity(identity)
{
    setWindowTitle(tr("Welcome to Ward Planning"));
    setMinimumWidth(460);

    QVBoxLayout* layout = new QVBoxLayout(this);
    m_pages = new QStackedWidget(this);
    layout->addWidget(m_pages);

    // The question.
    QWidget* question = new QWidget(m_pages);
    QVBoxLayout* questionLayout = new QVBoxLayout(question);
    QLabel* asked = new QLabel(tr("Do you already use Ward Planning on another device?"), question);
    asked->setWordWrap(true);
    QFont larger = asked->font();
    larger.setPointSizeF(larger.pointSizeF() * 1.25);
    asked->setFont(larger);
    questionLayout->addWidget(asked);
    QLabel* why = new QLabel(tr("If you do, this device joins you there, so the wards you share see one of you, "
                                "not two."),
                             question);
    why->setWordWrap(true);
    questionLayout->addWidget(why);
    QDialogButtonBox* answers = new QDialogButtonBox(question);
    QPushButton* yes = answers->addButton(tr("Yes, link this device"), QDialogButtonBox::ActionRole);
    QPushButton* no = answers->addButton(tr("No, this is my first"), QDialogButtonBox::ActionRole);
    answers->addButton(QDialogButtonBox::Cancel);
    questionLayout->addWidget(answers);
    m_pages->addWidget(question);

    // The names, for a new person.
    QWidget* names = new QWidget(m_pages);
    QFormLayout* form = new QFormLayout(names);
    QLabel* namesWhy = new QLabel(tr("Others with access to your wards see these names."), names);
    namesWhy->setWordWrap(true);
    form->addRow(namesWhy);
    m_displayName = new QLineEdit(names);
    m_displayName->setPlaceholderText(tr("Your name, as your ward knows you"));
    form->addRow(tr("Your name:"), m_displayName);
    m_deviceName = new QLineEdit(IdentityService::defaultDeviceName(), names);
    form->addRow(tr("This device:"), m_deviceName);
    QDialogButtonBox* create = new QDialogButtonBox(names);
    m_createButton = create->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    QPushButton* back = create->addButton(tr("Back"), QDialogButtonBox::ResetRole);
    form->addRow(create);
    m_pages->addWidget(names);

    connect(yes, &QPushButton::clicked, this, &FirstRunDialog::onLinkDevice);
    connect(no, &QPushButton::clicked, this, &FirstRunDialog::onNewPerson);
    connect(answers, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(back, &QPushButton::clicked, this, [this]() { m_pages->setCurrentIndex(0); });
    connect(m_createButton, &QPushButton::clicked, this, &FirstRunDialog::onCreate);
    connect(m_displayName, &QLineEdit::textChanged, this, &FirstRunDialog::onNamesChanged);
    connect(m_deviceName, &QLineEdit::textChanged, this, &FirstRunDialog::onNamesChanged);
    onNamesChanged();
}

void FirstRunDialog::onNewPerson()
{
    m_pages->setCurrentIndex(1);
    m_displayName->setFocus();
}

void FirstRunDialog::onLinkDevice()
{
    QMessageBox::information(this,
                             tr("Link This Device"),
                             tr("Linking needs this device to show a code you enter on your other device. "
                                "This build cannot do that yet."));
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
