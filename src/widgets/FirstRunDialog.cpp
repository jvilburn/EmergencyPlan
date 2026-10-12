#include "FirstRunDialog.h"

#include "IdentityService.h"
#include "ServerConnection.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace ServerContract;
using namespace std::placeholders;

FirstRunDialog::FirstRunDialog(IdentityService& identity, ServerApi& server, QWidget* parent)
    : QDialog(parent)
    , m_identity(identity)
    , m_server(server)
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

    // Linking, for a person with another device.
    QWidget* linking = new QWidget(m_pages);
    QFormLayout* linkForm = new QFormLayout(linking);
    QLabel* linkWhy = new QLabel(tr("Name this device, then show a code to enter on the device you already use."),
                                 linking);
    linkWhy->setWordWrap(true);
    linkForm->addRow(linkWhy);
    m_linkDeviceName = new QLineEdit(IdentityService::defaultDeviceName(), linking);
    linkForm->addRow(tr("This device:"), m_linkDeviceName);
    m_codeLabel = new QLabel(linking);
    m_codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont codeFont = m_codeLabel->font();
    codeFont.setPointSizeF(codeFont.pointSizeF() * 2.0);
    codeFont.setBold(true);
    m_codeLabel->setFont(codeFont);
    m_codeLabel->setAlignment(Qt::AlignCenter);
    linkForm->addRow(m_codeLabel);
    m_linkStatus = new QLabel(linking);
    m_linkStatus->setWordWrap(true);
    linkForm->addRow(m_linkStatus);
    QDialogButtonBox* linkButtons = new QDialogButtonBox(linking);
    m_showCode = linkButtons->addButton(tr("Show Code"), QDialogButtonBox::ActionRole);
    QPushButton* linkBack = linkButtons->addButton(tr("Back"), QDialogButtonBox::ResetRole);
    linkForm->addRow(linkButtons);
    m_pages->addWidget(linking);

    m_linkCheck.setInterval(LinkCheckMilliseconds);
    connect(&m_linkCheck, &QTimer::timeout, this, &FirstRunDialog::checkLink);
    connect(m_showCode, &QPushButton::clicked, this, &FirstRunDialog::onShowCode);
    connect(linkBack, &QPushButton::clicked, this, &FirstRunDialog::onBack);

    connect(yes, &QPushButton::clicked, this, &FirstRunDialog::onLinkDevice);
    connect(no, &QPushButton::clicked, this, &FirstRunDialog::onNewPerson);
    connect(answers, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(back, &QPushButton::clicked, this, &FirstRunDialog::onBack);
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

FirstRunDialog::~FirstRunDialog() = default;

void FirstRunDialog::onLinkDevice()
{
    m_pages->setCurrentIndex(2);
    m_linkDeviceName->setFocus();
}

void FirstRunDialog::onBack()
{
    m_linkCheck.stop();
    m_connection.reset();
    m_code.clear();
    m_codeLabel->clear();
    m_linkStatus->clear();
    m_showCode->setEnabled(true);
    m_pages->setCurrentIndex(0);
}

void FirstRunDialog::onShowCode()
{
    if (m_linkDeviceName->text().trimmed().isEmpty())
    {
        m_linkStatus->setText(tr("Name this device first."));
        return;
    }

    m_showCode->setEnabled(false);
    m_linkStatus->setText(tr("Asking the server for a code..."));
    m_connection = std::make_unique<ServerConnection>(m_server, m_identity.deviceOnly(m_linkDeviceName->text()));
    m_connection->startLink(this, std::bind(&FirstRunDialog::onCode, this, _1));
}

void FirstRunDialog::onCode(const Result<QString>& code)
{
    if (!code.ok())
    {
        linkFailed(tr("The server could not be reached. Connect to the internet and try again."));
        return;
    }

    m_code = code.value.value();
    m_codeLabel->setText(m_code);
    m_linkStatus->setText(tr("On a device you already use, open Who Has Access, choose Link a Device, and enter "
                             "this code. It works for ten minutes."));
    m_linkCheck.start();
}

void FirstRunDialog::checkLink()
{
    if (m_connection && !m_code.isEmpty())
    {
        m_connection->link(m_code, this, std::bind(&FirstRunDialog::onLink, this, _1));
    }
}

void FirstRunDialog::onLink(const Result<LinkState>& state)
{
    if (state.error == ServerError::Expired || state.error == ServerError::NotFound)
    {
        linkFailed(tr("The code expired before it was used. Show a new one."));
        return;
    }
    if (!state.ok() || !state.value->approvedAs.has_value())
    {
        // Not yet approved, or not reached: asked again shortly.
        return;
    }

    m_linkCheck.stop();
    const ServerContract::Person& person = state.value->approvedAs.value();
    QString error;
    if (!m_identity.joinPerson(person.member, person.displayName, m_linkDeviceName->text(), &error))
    {
        linkFailed(error);
        return;
    }
    accept();
}

void FirstRunDialog::linkFailed(const QString& message)
{
    m_linkCheck.stop();
    m_connection.reset();
    m_code.clear();
    m_codeLabel->clear();
    m_linkStatus->setText(message);
    m_showCode->setEnabled(true);
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
