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

    // What the lease promises holds only while connected, so the words say so.
    QString text;
    QString detail;
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
        detail = tr("Your copy on this device is untouched. %1").arg(session->faultReason());
        break;
    }

    if (session->isDirectoryReadOnly())
    {
        text = tr("Another member is editing the directory; you can still record emergency responses.");
        detail = tr("While you are connected, one person edits the directory at a time.");
    }

    setText(text);
    setToolTip(detail);
    show();
}
