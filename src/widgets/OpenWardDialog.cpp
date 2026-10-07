#include "OpenWardDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

OpenWardDialog::OpenWardDialog(const QList<DocumentLibrary::Entry>& entries, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Open Ward"));
    resize(420, 360);

    QVBoxLayout* layout = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    for (const DocumentLibrary::Entry& entry : entries)
    {
        const QString where = entry.shared ? tr("Shared") : tr("On this device only");
        QListWidgetItem* item = new QListWidgetItem(tr("%1  -  %2").arg(entry.wardName, where), m_list);
        item->setData(Qt::UserRole, entry.documentId);
        if (entry.lastOpened.has_value())
        {
            item->setToolTip(tr("Last opened %1").arg(QLocale().toString(entry.lastOpened.value().toLocalTime(),
                                                                            QLocale::ShortFormat)));
        }
    }

    if (entries.isEmpty())
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
    m_openButton->setEnabled(!m_list->selectedItems().isEmpty());
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
