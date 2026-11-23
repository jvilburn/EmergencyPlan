#pragma once

#include "UpdateCheck.h"

#include <QDialog>
#include <QNetworkAccessManager>
#include <QSaveFile>
#include <QString>

class QNetworkReply;
class QProgressBar;

/// Downloads a release's installer, showing how far it has come. It is accepted once the
/// installer is saved, and rejected when the user cancels or the download fails, which it says.
class UpdateDownloadDialog : public QDialog
{
    Q_OBJECT

public:
    /// Starts downloading release's installer into the temporary folder.
    UpdateDownloadDialog(const Release& release, QWidget* parent);

    /// Where the installer was saved, once the dialog was accepted.
    QString installerPath() const { return m_file.fileName(); }

private slots:
    void onReadyRead();
    void onProgress(qint64 received, qint64 total);
    void onFinished();

private:
    /// A download that receives nothing for this long is given up.
    static constexpr int StalledMilliseconds = 30000;

    QNetworkAccessManager m_network;
    QNetworkReply* m_reply;  ///< owned by m_network
    QSaveFile m_file;
    QProgressBar* m_progress;
};
