#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QDateTime>

class QListWidget;
class QPushButton;

/// Summary info extracted from an archive file.
struct ArchiveEntry
{
    QString filePath;
    QString name;
    QDateTime startedAt;
    std::optional<QDateTime> endedAt;
    int familyCount = 0;
    int taskCount = 0;
};

/// Dialog for browsing emergency archives stored alongside the document.
/// Shows a list of archived emergencies with View, Reopen, Delete actions.
class ArchiveBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Action
    {
        None,
        View,
        Reopen
    };

    explicit ArchiveBrowserDialog(const QString& archiveDir, QWidget* parent);

    Action selectedAction() const { return m_selectedAction; }
    QString selectedFilePath() const;

private slots:
    void onSelectionChanged();
    void onViewClicked();
    void onReopenClicked();
    void onDeleteClicked();
    void onItemDoubleClicked();

private:
    void loadArchives();
    void refreshList();
    int currentIndex() const;

    QString m_archiveDir;
    QList<ArchiveEntry> m_entries;
    Action m_selectedAction = Action::None;

    QListWidget* m_listWidget;
    QPushButton* m_viewButton;
    QPushButton* m_reopenButton;
    QPushButton* m_deleteButton;
    QPushButton* m_closeButton;
};
