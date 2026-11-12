#include "OpenWardDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

OpenWardDialog::OpenWardDialog(const QList<WardListEntry>& wards, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Wards"));
    resize(420, 320);

    QVBoxLayout* layout = new QVBoxLayout(this);
    if (wards.isEmpty())
    {
        QLabel* empty = new QLabel(
            tr("This device holds no wards yet. Create one with New Ward, or open an invitation with Join a Ward."),
            this);
        empty->setWordWrap(true);
        layout->addWidget(empty);
    }

    m_list = new QListWidget(this);
    for (const WardListEntry& ward : wards)
    {
        QListWidgetItem* item = new QListWidgetItem(m_list);
        if (ward.status != OpenStatus::Opened)
        {
            item->setText(openFailureMessage(ward.status));
            item->setFlags(Qt::NoItemFlags);
            continue;
        }
        if (ward.noLongerShared)
        {
            item->setText(tr("%1 (no longer shared)").arg(ward.name));
        }
        else if (ward.changesToShare)
        {
            item->setText(tr("%1 (changes to share)").arg(ward.name));
        }
        else
        {
            item->setText(ward.name);
        }
        item->setData(Qt::UserRole, ward.document.toString());
    }
    layout->addWidget(m_list);

    QDialogButtonBox* buttons = new QDialogButtonBox(this);
    m_openButton = buttons->addButton(tr("Open"), QDialogButtonBox::AcceptRole);
    QPushButton* newWardButton = buttons->addButton(tr("New Ward..."), QDialogButtonBox::ActionRole);
    QPushButton* joinButton = buttons->addButton(tr("Join a Ward..."), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &OpenWardDialog::onOpen);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(newWardButton, &QPushButton::clicked, this, &OpenWardDialog::onNewWard);
    connect(joinButton, &QPushButton::clicked, this, &OpenWardDialog::onJoin);
    connect(m_list, &QListWidget::itemActivated, this, &OpenWardDialog::onItemActivated);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &OpenWardDialog::updateOpenButton);

    if (!wards.isEmpty() && wards.first().status == OpenStatus::Opened)
    {
        m_list->setCurrentRow(0);
    }
    updateOpenButton();
}

WardListChoice OpenWardDialog::choose()
{
    switch (exec())
    {
    case QDialog::Accepted:
        return WardListChoice::Open;
    case NewWardResult:
        return WardListChoice::NewWard;
    case JoinResult:
        return WardListChoice::Join;
    default:
        return WardListChoice::Close;
    }
}

void OpenWardDialog::onOpen()
{
    QList<QListWidgetItem*> selected = m_list->selectedItems();
    if (!selected.isEmpty())
    {
        openItem(selected.constFirst());
    }
}

void OpenWardDialog::onItemActivated(QListWidgetItem* item)
{
    openItem(item);
}

void OpenWardDialog::openItem(const QListWidgetItem* item)
{
    m_chosenWard = DocumentId::fromString(item->data(Qt::UserRole).toString());
    accept();
}

void OpenWardDialog::onNewWard()
{
    done(NewWardResult);
}

void OpenWardDialog::onJoin()
{
    done(JoinResult);
}

void OpenWardDialog::updateOpenButton()
{
    m_openButton->setEnabled(!m_list->selectedItems().isEmpty());
}

QString OpenWardDialog::openFailureMessage(OpenStatus status)
{
    switch (status)
    {
    case OpenStatus::Opened:
        return QString();
    case OpenStatus::Missing:
        return tr("This ward is no longer on this device.");
    case OpenStatus::CannotRead:
        return tr("This ward's file on this device could not be opened.");
    case OpenStatus::Newer:
        return tr("This ward was saved by a newer version of Ward Planning. Update to open it.");
    case OpenStatus::Fault:
        return tr("This ward's file could not be read just now.");
    }
    return QString();
}
