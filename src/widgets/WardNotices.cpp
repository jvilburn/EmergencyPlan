#include "WardNotices.h"

#include "DocumentManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

WardNotices::WardNotices(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    DocumentManager* manager = DocumentManager::instance();
    connect(manager, &DocumentManager::documentOpened, this, &WardNotices::onDocumentOpened);
    connect(manager, &DocumentManager::requiresUpdateChanged, this, &WardNotices::refresh);
    connect(manager, &DocumentManager::accessChanged, this, &WardNotices::onAccessChanged);
    connect(manager, &DocumentManager::documentChanged, this, &WardNotices::refresh);
    refresh();
}

void WardNotices::onDocumentOpened()
{
    // Another ward's changes are not this one's.
    m_accessEvents.clear();
    refresh();
}

void WardNotices::onAccessChanged(const QList<AccessEvents::Event>& events)
{
    // A new sync's changes are news even after the last notice was dismissed.
    m_accessEvents = events;
    m_dismissed.remove(dismissKey(Kind::AccessChanged));
    refresh();
}

void WardNotices::refresh()
{
    // Later, since the button pressed to dismiss a notice is inside it.
    while (QLayoutItem* item = m_layout->takeAt(0))
    {
        item->widget()->hide();
        item->widget()->deleteLater();
        delete item;
    }

    const DocumentManager* manager = DocumentManager::instance();
    const DocumentSession* session = manager->session();
    if (session == nullptr || manager->copy().has_value())
    {
        hide();
        return;
    }

    const AccessRoster& roster = manager->document().accessRoster();
    const std::optional<RosterPerson> self = roster.person(manager->identity().member);
    QList<QWidget*> notices;

    if (session->requiresUpdate())
    {
        notices.append(makeNotice(Kind::RequiresUpdate,
                                  tr("This ward was saved by a newer version of Ward Planning. Update to see the "
                                     "latest changes and share your work."),
                                  QString(),
                                  nullptr));
    }

    if (roster.personCount() == 1)
    {
        notices.append(makeNotice(Kind::OnlyKeyHolder,
                                  tr("Only you can open this ward. If you lose your devices, it cannot be "
                                     "recovered. Add someone now."),
                                  tr("Add Someone..."),
                                  &WardNotices::accessRequested));
    }
    else if (self.has_value() && self.value().isAdmin() && roster.admins().size() == 1)
    {
        notices.append(makeNotice(Kind::OnlyAdmin,
                                  tr("You are the only admin. If you lose your devices, nobody can add or remove "
                                     "people. Make someone else an admin."),
                                  tr("Make Someone an Admin..."),
                                  &WardNotices::accessRequested));
    }

    if (self.has_value() && self.value().deviceCount() == 1 && session->isShared())
    {
        notices.append(makeNotice(Kind::OneDevice,
                                  tr("This ward is on one of your devices. Linking a second keeps it if this one "
                                     "is lost."),
                                  tr("Link a Device..."),
                                  &WardNotices::linkRequested));
    }

    if (!m_accessEvents.isEmpty())
    {
        QStringList lines;
        for (const AccessEvents::Event& event : m_accessEvents)
        {
            lines.append(event.describe());
        }
        QWidget* notice = makeNotice(Kind::AccessChanged,
                                     tr("%n change(s) to who has access", "", int(m_accessEvents.size())),
                                     QString(),
                                     nullptr);
        notice->setToolTip(lines.join("\n"));

        // The list itself, under the count, so it is read without a click.
        QLabel* list = new QLabel(lines.join("\n"), notice);
        list->setContentsMargins(24, 0, 12, 6);
        static_cast<QVBoxLayout*>(notice->layout())->addWidget(list);
        notices.append(notice);
    }

    for (QWidget* notice : notices)
    {
        if (notice != nullptr)
        {
            m_layout->addWidget(notice);
        }
    }
    setVisible(m_layout->count() > 0);
}

QWidget* WardNotices::makeNotice(Kind kind, const QString& text, const QString& button, void (WardNotices::*pressed)())
{
    if (m_dismissed.contains(dismissKey(kind)))
    {
        return nullptr;
    }

    QWidget* notice = new QWidget(this);
    notice->setObjectName("wardNotice");
    QVBoxLayout* column = new QVBoxLayout(notice);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    QHBoxLayout* row = new QHBoxLayout();
    row->setContentsMargins(12, 6, 6, 6);
    QLabel* label = new QLabel(text, notice);
    label->setWordWrap(true);
    row->addWidget(label, 1);
    if (!button.isEmpty() && pressed != nullptr)
    {
        QPushButton* action = new QPushButton(button, notice);
        row->addWidget(action);
        connect(action, &QPushButton::clicked, this, pressed);
    }
    QToolButton* close = new QToolButton(notice);
    close->setText(QStringLiteral("x"));
    close->setToolTip(tr("Dismiss"));
    close->setAutoRaise(true);
    row->addWidget(close);
    column->addLayout(row);

    connect(close, &QToolButton::clicked, this, std::bind(&WardNotices::dismiss, this, kind));
    return notice;
}

void WardNotices::dismiss(Kind kind)
{
    m_dismissed.insert(dismissKey(kind));
    if (kind == Kind::AccessChanged)
    {
        m_accessEvents.clear();
    }
    refresh();
}

QString WardNotices::dismissKey(Kind kind) const
{
    return DocumentManager::instance()->documentId() + "/" + QString::number(int(kind));
}
