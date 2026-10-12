#include "WardNotices.h"

#include "DocumentManager.h"
#include "UpdateChecker.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>



WardNotices::WardNotices(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    DocumentManager* manager = DocumentManager::instance();
    connect(manager, &DocumentManager::documentOpened, this, &WardNotices::refresh);
    connect(manager, &DocumentManager::requiresUpdateChanged, this, &WardNotices::refresh);
    connect(manager, &DocumentManager::headChanged, this, &WardNotices::refresh);
    connect(manager, &DocumentManager::accessChanged, this, &WardNotices::refresh);
    refresh();
}

void WardNotices::setUpdateChecker(UpdateChecker* updates)
{
    m_updates = updates;
    connect(updates, &UpdateChecker::changed, this, &WardNotices::refresh);
    refresh();
}

QList<WardNotices::Notice> WardNotices::currentNotices() const
{
    QList<Notice> notices;
    if (m_updates != nullptr && !m_updates->newerVersion().isEmpty())
    {
        notices.append(Notice{Kind::UpdateAvailable,
                              tr("Ward Planning %1 is available.").arg(m_updates->newerVersion()),
                              QString()});
    }

    const DocumentManager* manager = DocumentManager::instance();
    const DocumentSession* session = manager->session();
    if (session == nullptr || manager->copy().has_value())
    {
        return notices;
    }

    if (session->requiresUpdate())
    {
        notices.append(Notice{Kind::RequiresUpdate,
                              tr("This ward was saved by a newer version of Ward Planning. Update to see the latest "
                                 "changes and share your work."),
                              QString()});
    }

    // The roster is the ward's own; an unshared ward's is this person alone.
    const SealingSet roster = manager->roster();
    const std::optional<Holder> self = manager->self();

    if (!session->isShared() || roster.people().size() == 1)
    {
        notices.append(Notice{Kind::OnlyKeyHolder,
                              tr("Only you can open this ward. If you lose your devices, it cannot be recovered. "
                                 "Add someone now."),
                              QString()});
    }
    else if (self.has_value() && self->isAdmin() && roster.adminCount() == 1)
    {
        notices.append(Notice{Kind::OnlyAdmin,
                              tr("You are the only admin. If you lose your devices, nobody can add or remove people. "
                                 "Make someone else an admin."),
                              QString()});
    }

    if (session->isShared() && self.has_value() && self->devices.size() == 1)
    {
        notices.append(Notice{Kind::OneDevice,
                              tr("This ward is on one of your devices. Adding a second keeps it if this one is lost."),
                              QString()});
    }

    const QStringList changes = manager->unseenRosterChanges();
    if (!changes.isEmpty())
    {
        notices.append(Notice{Kind::AccessChanged,
                              tr("%n change(s) to who has access", "", int(changes.size())),
                              changes.join("\n")});
    }

    const QSet<Kind> dismissed = m_dismissed.value(manager->documentId());
    notices.removeIf([&dismissed](const Notice& notice) { return dismissed.contains(notice.kind); });
    return notices;
}

void WardNotices::refresh()
{
    // Rebuilt only when what shows changes, so a button under the mouse survives
    // every refresh that changes nothing.
    const QList<Notice> notices = currentNotices();
    if (notices == m_shown)
    {
        return;
    }
    m_shown = notices;

    // Later, since the button pressed to dismiss a notice is inside it.
    while (QLayoutItem* item = m_layout->takeAt(0))
    {
        item->widget()->hide();
        item->widget()->deleteLater();
        delete item;
    }

    for (const Notice& notice : notices)
    {
        m_layout->addWidget(makeNotice(notice));
    }
    setVisible(!notices.isEmpty());
}

QWidget* WardNotices::makeNotice(const Notice& notice)
{
    QFrame* frame = new QFrame(this);
    frame->setObjectName("wardNotice");
    frame->setStyleSheet("QFrame#wardNotice {"
                         "  background-color: #FFF4CE;"
                         "  border-bottom: 1px solid #E5D38F;"
                         "}");
    QVBoxLayout* column = new QVBoxLayout(frame);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    QHBoxLayout* row = new QHBoxLayout();
    row->setContentsMargins(12, 6, 6, 6);
    QLabel* label = new QLabel(notice.text, frame);
    label->setWordWrap(true);
    row->addWidget(label, 1);

    switch (notice.kind)
    {
    case Kind::UpdateAvailable:
    {
        QPushButton* download = new QPushButton(tr("Download..."), frame);
        QPushButton* later = new QPushButton(tr("Not Now"), frame);
        row->addWidget(download);
        row->addWidget(later);
        connect(download, &QPushButton::clicked, this, &WardNotices::openReleasePage);
        connect(later, &QPushButton::clicked, m_updates, &UpdateChecker::dismiss);
        break;
    }
    case Kind::OnlyKeyHolder:
    case Kind::OnlyAdmin:
    {
        QPushButton* action = new QPushButton(
            notice.kind == Kind::OnlyKeyHolder ? tr("Add Someone...") : tr("Make Someone an Admin..."), frame);
        row->addWidget(action);
        connect(action, &QPushButton::clicked, this, &WardNotices::accessRequested);
        break;
    }
    case Kind::OneDevice:
    {
        QPushButton* action = new QPushButton(tr("Add a Device..."), frame);
        row->addWidget(action);
        connect(action, &QPushButton::clicked, this, &WardNotices::addDeviceRequested);
        break;
    }
    case Kind::RequiresUpdate:
    case Kind::AccessChanged:
        break;
    }

    // The update notices have their own buttons: Not Now, or updating the app.
    if (notice.kind != Kind::RequiresUpdate && notice.kind != Kind::UpdateAvailable)
    {
        QToolButton* close = new QToolButton(frame);
        close->setText(QStringLiteral("x"));
        close->setToolTip(tr("Dismiss"));
        close->setAutoRaise(true);
        row->addWidget(close);
        connect(close, &QToolButton::clicked, this, std::bind(&WardNotices::dismiss, this, notice.kind));
    }
    column->addLayout(row);

    // The list itself, under the count, so it is read without a click.
    if (!notice.details.isEmpty())
    {
        QLabel* list = new QLabel(notice.details, frame);
        list->setContentsMargins(24, 0, 12, 6);
        column->addWidget(list);
    }
    return frame;
}

void WardNotices::openReleasePage()
{
    QDesktopServices::openUrl(UpdateChecker::releasePage());
}

void WardNotices::dismiss(Kind kind)
{
    DocumentManager* manager = DocumentManager::instance();
    if (kind == Kind::AccessChanged)
    {
        // Seen for good, on the record, so the next sync shows only what is newer.
        manager->dismissRosterChanges();
        return;
    }
    m_dismissed[manager->documentId()].insert(kind);
    refresh();
}
