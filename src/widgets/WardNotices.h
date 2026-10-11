#pragma once

#include "AccessEvents.h"

#include <QList>
#include <QSet>
#include <QWidget>

class QVBoxLayout;

/// The open ward's standing notices, under the emergency banner: each persistent
/// until dismissed, never a per-save interruption.
///
/// - Only you can open this ward: lose your devices and it cannot be recovered.
/// - With one device, a gentler suggestion to link a second.
/// - You are the only admin while others remain.
/// - The ward was saved by a newer version of Ward Planning.
/// - Who has access changed: one notice per sync, expanding to the list.
///
/// A dismissed notice stays dismissed for that ward while the app runs.
class WardNotices : public QWidget
{
    Q_OBJECT

public:
    explicit WardNotices(QWidget* parent);

signals:
    /// The notice's own button: add someone, make someone an admin.
    void accessRequested();
    void linkRequested();

public slots:
    void refresh();

private slots:
    void onDocumentOpened();
    void onAccessChanged(const QList<AccessEvents::Event>& events);

private:
    enum class Kind
    {
        OnlyKeyHolder,
        OneDevice,
        OnlyAdmin,
        RequiresUpdate,
        AccessChanged
    };

    /// One notice: its text, an optional button, and a dismiss button.
    QWidget* makeNotice(Kind kind, const QString& text, const QString& button, void (WardNotices::*pressed)());
    void dismiss(Kind kind);
    QString dismissKey(Kind kind) const;

    QVBoxLayout* m_layout = nullptr;
    QSet<QString> m_dismissed;
    QList<AccessEvents::Event> m_accessEvents;
};
