#include "SettingsDialog.h"
#include "AppSettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const AppSettings& settings, const Coordinates& currentView, QWidget* parent)
    : QDialog(parent)
    , m_currentView(currentView)
{
    setWindowTitle(tr("Settings"));

    QVBoxLayout* layout = new QVBoxLayout(this);

    m_useServer = new QCheckBox(tr("Use the Ward Planning server"), this);
    m_useServer->setChecked(settings.useServer());
    layout->addWidget(m_useServer);

    QLabel* serverLine = new QLabel(tr("Turn this off only if the Ward Planning server has shut down. Every member "
                                       "of your wards must turn it off too."),
                                    this);
    serverLine->setWordWrap(true);
    layout->addWidget(serverLine);

    QFormLayout* form = new QFormLayout();
    m_deviceName = new QLineEdit(settings.deviceName(), this);
    form->addRow(tr("This device's name"), m_deviceName);
    m_userName = new QLineEdit(settings.userName(), this);
    form->addRow(tr("Your name"), m_userName);
    m_email = new QLineEdit(settings.email(), this);
    form->addRow(tr("Your email"), m_email);

    QHBoxLayout* mapHomeRow = new QHBoxLayout();
    m_mapHomeLabel = new QLabel(this);
    QPushButton* useCurrentView = new QPushButton(tr("Use current view"), this);
    mapHomeRow->addWidget(m_mapHomeLabel, 1);
    mapHomeRow->addWidget(useCurrentView);
    form->addRow(tr("Map home"), mapHomeRow);
    layout->addLayout(form);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(useCurrentView, &QPushButton::clicked, this, &SettingsDialog::onUseCurrentView);
    connect(m_deviceName, &QLineEdit::textChanged, this, &SettingsDialog::updateOkButton);
    connect(m_userName, &QLineEdit::textChanged, this, &SettingsDialog::updateOkButton);

    showMapHome(settings.mapHome());
    updateOkButton();
}

bool SettingsDialog::useServer() const
{
    return m_useServer->isChecked();
}

QString SettingsDialog::deviceName() const
{
    return m_deviceName->text().trimmed();
}

QString SettingsDialog::userName() const
{
    return m_userName->text().trimmed();
}

QString SettingsDialog::email() const
{
    return m_email->text().trimmed();
}

void SettingsDialog::onUseCurrentView()
{
    m_mapHome = m_currentView;
    showMapHome(m_mapHome);
}

void SettingsDialog::updateOkButton()
{
    m_okButton->setEnabled(!deviceName().isEmpty() && !userName().isEmpty());
}

void SettingsDialog::showMapHome(const std::optional<Coordinates>& home)
{
    if (!home)
    {
        m_mapHomeLabel->setText(tr("Not set"));
        return;
    }
    m_mapHomeLabel->setText(
        tr("%1, %2").arg(home->latitude(), 0, 'f', 4).arg(home->longitude(), 0, 'f', 4));
}
