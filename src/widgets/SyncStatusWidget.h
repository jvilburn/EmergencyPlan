#pragma once

#include <QLabel>

class UpdateChecker;

/// The open ward's sync state, quiet and persistent in the status bar: up to date,
/// changes not yet uploaded, server unreachable, the server's copy not checked, two
/// versions waiting for a choice, or a newer version of the app needed - and beside
/// any of them, while another member holds the lease, that they are editing the
/// directory. Nothing for an unshared ward or a copy. Never a dialog, never takes
/// focus.
class SyncStatusWidget : public QLabel
{
    Q_OBJECT

public:
    explicit SyncStatusWidget(QWidget* parent);

    /// While the server is unreachable and a newer version is known, says so: an
    /// old build is one reason a server stops answering.
    void setUpdateChecker(UpdateChecker* updates);

public slots:
    /// Reads the state from the open ward's session.
    void refresh();

private:
    UpdateChecker* m_updates = nullptr;
};
