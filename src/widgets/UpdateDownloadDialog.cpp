#include "UpdateDownloadDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QStandardPaths>
#include <QVBoxLayout>

UpdateDownloadDialog::UpdateDownloadDialog(const Release& release, QWidget* parent)
    : QDialog(parent)
    , m_file(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                 .filePath(QString("WardPlanning-%1-setup.exe").arg(release.version)))
{
    setWindowTitle(tr("Update"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Downloading Ward Planning %1...").arg(release.version), this));
    m_progress = new QProgressBar(this);
    m_progress->setMinimumWidth(320);
    // The bar runs without a measure until the download says how long it is.
    m_progress->setRange(0, 0);
    layout->addWidget(m_progress);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_file.open(QIODevice::WriteOnly);
    QNetworkRequest request(release.installer);
    request.setTransferTimeout(StalledMilliseconds);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &UpdateDownloadDialog::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &UpdateDownloadDialog::onProgress);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateDownloadDialog::onFinished);
}

void UpdateDownloadDialog::onReadyRead()
{
    m_file.write(m_reply->readAll());
}

void UpdateDownloadDialog::onProgress(qint64 received, qint64 total)
{
    if (total > 0)
    {
        m_progress->setRange(0, 1000);
        m_progress->setValue(int(received * 1000 / total));
    }
}

void UpdateDownloadDialog::onFinished()
{
    m_file.write(m_reply->readAll());
    if (m_reply->error() != QNetworkReply::NoError || !m_file.commit())
    {
        QMessageBox::warning(this, tr("Update"),
                             tr("The update could not be downloaded. Ward Planning will offer it again the next "
                                "time it starts."));
        reject();
        return;
    }
    accept();
}
