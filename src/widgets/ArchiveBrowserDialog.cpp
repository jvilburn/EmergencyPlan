#include "ArchiveBrowserDialog.h"
#include "JsonService.h"
#include "EmergencyResponse.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>

#include <algorithm>

ArchiveBrowserDialog::ArchiveBrowserDialog(const QString& archiveDir, QWidget* parent)
    : QDialog(parent)
    , m_archiveDir(archiveDir)
{
    setWindowTitle(tr("Emergency Archives"));
    setMinimumSize(500, 350);

    QVBoxLayout* layout = new QVBoxLayout(this);

    m_listWidget = new QListWidget(this);
    m_listWidget->setAlternatingRowColors(true);
    layout->addWidget(m_listWidget);

    // Button row
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_viewButton = new QPushButton(tr("View"), this);
    m_reopenButton = new QPushButton(tr("Reopen"), this);
    m_deleteButton = new QPushButton(tr("Delete"), this);
    m_closeButton = new QPushButton(tr("Close"), this);

    buttonLayout->addWidget(m_viewButton);
    buttonLayout->addWidget(m_reopenButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);

    layout->addLayout(buttonLayout);

    // Connections
    connect(m_listWidget, &QListWidget::currentRowChanged,
            this, &ArchiveBrowserDialog::onSelectionChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &ArchiveBrowserDialog::onItemDoubleClicked);
    connect(m_viewButton, &QPushButton::clicked,
            this, &ArchiveBrowserDialog::onViewClicked);
    connect(m_reopenButton, &QPushButton::clicked,
            this, &ArchiveBrowserDialog::onReopenClicked);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &ArchiveBrowserDialog::onDeleteClicked);
    connect(m_closeButton, &QPushButton::clicked,
            this, &QDialog::reject);

    loadArchives();
    refreshList();
    onSelectionChanged();
}

QString ArchiveBrowserDialog::selectedFilePath() const
{
    int idx = currentIndex();
    if (idx < 0 || idx >= m_entries.size())
    {
        return {};
    }
    return m_entries[idx].filePath;
}

void ArchiveBrowserDialog::onSelectionChanged()
{
    bool hasSelection = currentIndex() >= 0;
    m_viewButton->setEnabled(hasSelection);
    m_reopenButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
}

void ArchiveBrowserDialog::onViewClicked()
{
    if (currentIndex() < 0)
    {
        return;
    }
    m_selectedAction = Action::View;
    accept();
}

void ArchiveBrowserDialog::onReopenClicked()
{
    if (currentIndex() < 0)
    {
        return;
    }

    const ArchiveEntry& entry = m_entries[currentIndex()];
    int result = QMessageBox::question(
        this,
        tr("Reopen Emergency"),
        tr("Reopen \"%1\" as the active emergency?\n\n"
           "This will load the archived response data back into your document.")
            .arg(entry.name),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes)
    {
        m_selectedAction = Action::Reopen;
        accept();
    }
}

void ArchiveBrowserDialog::onDeleteClicked()
{
    int idx = currentIndex();
    if (idx < 0)
    {
        return;
    }

    const ArchiveEntry& entry = m_entries[idx];
    int result = QMessageBox::warning(
        this,
        tr("Delete Archive"),
        tr("Permanently delete the archive \"%1\"?\n\nThis cannot be undone.")
            .arg(entry.name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result == QMessageBox::Yes)
    {
        if (QFile::remove(entry.filePath))
        {
            m_entries.removeAt(idx);
            refreshList();
        }
        else
        {
            QMessageBox::critical(
                this,
                tr("Delete Failed"),
                tr("Could not delete the archive file."));
        }
    }
}

void ArchiveBrowserDialog::onItemDoubleClicked()
{
    onViewClicked();
}

void ArchiveBrowserDialog::loadArchives()
{
    m_entries.clear();

    QDir dir(m_archiveDir);
    if (!dir.exists())
    {
        return;
    }

    QStringList files = dir.entryList({"*.emergencyplan"}, QDir::Files, QDir::Name);

    for (const QString& fileName : files)
    {
        QString filePath = dir.absoluteFilePath(fileName);
        JsonResult result = JsonService::loadDocument(filePath);
        if (!result.success)
        {
            continue;
        }

        const std::optional<EmergencyResponse>& response = result.document.emergencyResponse();
        if (!response.has_value())
        {
            continue;
        }

        ArchiveEntry entry;
        entry.filePath = filePath;
        entry.name = response->name();
        entry.startedAt = response->startedAt();
        entry.endedAt = response->endedAt();
        entry.familyCount = response->totalFamilies();

        // Count total tasks across all family records
        int taskCount = 0;
        for (auto it = response->familyRecords().constBegin();
             it != response->familyRecords().constEnd(); ++it)
        {
            taskCount += it.value().tasks().size();
        }
        entry.taskCount = taskCount;

        m_entries.append(entry);
    }

    // Sort by start date, most recent first
    std::sort(m_entries.begin(), m_entries.end(),
              [](const ArchiveEntry& a, const ArchiveEntry& b)
              { return a.startedAt > b.startedAt; });
}

void ArchiveBrowserDialog::refreshList()
{
    m_listWidget->clear();

    for (const ArchiveEntry& entry : m_entries)
    {
        QString dateRange;
        if (entry.endedAt.has_value())
        {
            if (entry.startedAt.date() == entry.endedAt->date())
            {
                dateRange = entry.startedAt.toString("MMM d, yyyy");
            }
            else
            {
                dateRange = entry.startedAt.toString("MMM d")
                            + " - "
                            + entry.endedAt->toString("MMM d, yyyy");
            }
        }
        else
        {
            dateRange = entry.startedAt.toString("MMM d, yyyy");
        }

        QString line1 = entry.name + " - " + dateRange;
        QString line2 = tr("  %1 families, %2 tasks")
                             .arg(entry.familyCount)
                             .arg(entry.taskCount);

        m_listWidget->addItem(line1 + "\n" + line2);
    }

    if (m_entries.isEmpty())
    {
        m_listWidget->addItem(tr("No archives found"));
        m_listWidget->item(0)->setFlags(Qt::NoItemFlags);
    }
}

int ArchiveBrowserDialog::currentIndex() const
{
    return m_listWidget->currentRow();
}
