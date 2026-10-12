#include "OpenWardDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

OpenWardDialog::OpenWardDialog(const QList<DocumentLibrary::Entry>& entries,
                               const QStringList& requested,
                               const QStringList& waiting,
                               QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Open Ward"));
    resize(420, 360);

    QVBoxLayout* layout = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    for (const DocumentLibrary::Entry& entry : entries)
    {
        QString where = entry.shared ? tr("Shared") : tr("On this device only");
        if (entry.shared && !entry.onDevice)
        {
            where = tr("Shared, not yet on this device");
        }
        QListWidgetItem* item = new QListWidgetItem(tr("%1  -  %2").arg(entry.wardName, where), m_list);
        item->setData(Qt::UserRole, entry.documentId);
        item->setData(FetchRole, entry.shared && !entry.onDevice);
        item->setData(NameRole, entry.wardName);
        if (entry.lastOpened.has_value())
        {
            item->setToolTip(tr("Last opened %1").arg(QLocale().toString(entry.lastOpened.value().toLocalTime(),
                                                                            QLocale::ShortFormat)));
        }
    }

    for (qsizetype i = 0; i < requested.size(); ++i)
    {
        QListWidgetItem* item = new QListWidgetItem(tr("A ward you asked for  -  Requested"), m_list);
        item->setFlags(Qt::ItemIsEnabled);
        item->setToolTip(tr("It appears here once an admin grants your request."));
    }
    for (qsizetype i = 0; i < waiting.size(); ++i)
    {
        QListWidgetItem* item = new QListWidgetItem(tr("A ward shared with you  -  Waiting for access"), m_list);
        item->setFlags(Qt::ItemIsEnabled);
        item->setToolTip(tr("It arrives once the device that gave you access next runs Ward Planning."));
    }

    if (m_list->count() == 0)
    {
        layout->addWidget(new QLabel(tr("There are no wards on this device yet."), this));
        m_list->hide();
    }
    layout->addWidget(m_list);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_openButton = buttons->addButton(tr("Open"), QDialogButtonBox::AcceptRole);
    m_openButton->setEnabled(false);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::itemSelectionChanged, this, &OpenWardDialog::onSelectionChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &OpenWardDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::accepted, this, &OpenWardDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void OpenWardDialog::onSelectionChanged()
{
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    m_openButton->setEnabled(!selected.isEmpty());

    // A shared ward whose file is not here is fetched to open.
    if (!selected.isEmpty() && selected.first()->data(FetchRole).toBool())
    {
        m_openButton->setText(tr("Fetch %1 from the Server").arg(selected.first()->data(NameRole).toString()));
    }
    else
    {
        m_openButton->setText(tr("Open"));
    }
}

void OpenWardDialog::onAccepted()
{
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    if (selected.isEmpty())
    {
        return;
    }

    m_selected = selected.first()->data(Qt::UserRole).toString();
    accept();
}
