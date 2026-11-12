#pragma once

#include <QDialog>
#include <QString>

class QVBoxLayout;

/// A file ready to send: text for the user, which they can select and copy, then the file as
/// an icon with its name that drags into an email or message as the file itself, with a hint
/// saying so, then Show in Folder, which reveals the file, and Done.
class SendFileDialog : public QDialog
{
    Q_OBJECT

public:
    SendFileDialog(const QString& title, const QString& text, const QString& path, QWidget* parent);

protected:
    /// Adds widget below the text, above the file.
    void addAboveFile(QWidget* widget);

private:
    QVBoxLayout* m_aboveFile;
};
