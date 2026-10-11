#include "SyncStatusWidget.h"

#include "DocumentManager.h"

SyncStatusWidget::SyncStatusWidget(QWidget* parent)
    : QLabel(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setContentsMargins(8, 0, 8, 0);

    DocumentManager* manager = DocumentManager::instance();
    connect(manager, &DocumentManager::documentOpened, this, &SyncStatusWidget::refresh);
    connect(manager, &DocumentManager::syncStateChanged, this, &SyncStatusWidget::refresh);
    connect(manager, &DocumentManager::readOnlyChanged, this, &SyncStatusWidget::refresh);
    connect(manager, &DocumentManager::requiresUpdateChanged, this, &SyncStatusWidget::refresh);
    connect(manager, &DocumentManager::conflictRaised, this, &SyncStatusWidget::refresh);
    connect(manager, &DocumentManager::conflictSettled, this, &SyncStatusWidget::refresh);
    refresh();
}

void SyncStatusWidget::refresh()
{
    const DocumentSession* session = DocumentManager::instance()->session();
    if (session == nullptr || DocumentManager::instance()->copy().has_value() || !session->isShared())
    {
        hide();
        return;
    }

    // Each line promises only what holds: nothing waits to upload while a choice
    // or an update stands in the way, and the lease holds only while connected.
    QString text;
    QString detail;
    if (session->pendingConflict().has_value())
    {
        text = tr("Two versions are waiting for your choice");
        detail = tr("Nothing uploads until you choose. Use File > Choose Between Two Versions.");
    }
    else if (session->requiresUpdate())
    {
        text = tr("A newer version of Ward Planning is needed to sync this ward");
        detail = tr("Your changes are saved on this device and upload once Ward Planning is updated.");
    }
    else
    {
        switch (session->syncState())
        {
        case SyncState::Local:
            hide();
            return;
        case SyncState::UpToDate:
            text = tr("Up to date");
            break;
        case SyncState::PendingUpload:
            text = tr("Changes not yet uploaded");
            detail = tr("Your changes are saved on this device and upload when the server answers.");
            break;
        case SyncState::Unreachable:
            text = tr("Server unreachable");
            detail = tr("Your changes are saved on this device and upload when the server can be reached.");
            break;
        case SyncState::ServerFault:
            text = tr("Could not check the server's copy");
            detail = tr("Your copy on this device is untouched.");
            if (!session->faultReason().isEmpty())
            {
                detail += " " + session->faultReason();
            }
            break;
        }
    }

    // Beside the state, not instead of it: responses recorded meanwhile still sync.
    if (session->isDirectoryReadOnly())
    {
        text = tr("%1 - another member is editing the directory").arg(text);
        detail = tr("You can still record emergency responses. While you are connected, one person edits the "
                    "directory at a time.")
               + (detail.isEmpty() ? QString() : "\n" + detail);
    }

    setText(text);
    setToolTip(detail);
    show();
}
