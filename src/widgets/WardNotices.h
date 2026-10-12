#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class UpdateChecker;

/// The standing notices, under the emergency and copy banners: each persistent, never a
/// per-save interruption.
///
/// - A newer version of Ward Planning is published, with or without a ward open.
/// - The ward was saved by a newer version of Ward Planning, until it is updated.
/// - Only you can open this ward: lose your devices and it cannot be recovered.
/// - With one device, a gentler suggestion to link a second.
/// - You are the only admin while others remain.
/// - Who has access changed: the server's history since this device last dismissed
///   it, expanding to the list.
///
/// The others stay dismissed for that ward while the app runs; the access changes,
/// once dismissed, stay seen across runs.
class WardNotices : public QWidget
{
    Q_OBJECT

public:
    explicit WardNotices(QWidget* parent);

    /// Where the update notice reads from; without one there is no update notice.
    void setUpdateChecker(UpdateChecker* updates);

signals:
    /// The notice's own button: add someone, make someone an admin.
    void accessRequested();

    /// The notice suggesting a second device: link one from here.
    void linkRequested();

private:
    enum class Kind
    {
        UpdateAvailable,
        OnlyKeyHolder,
        OneDevice,
        OnlyAdmin,
        RequiresUpdate,
        AccessChanged
    };

    /// One notice as shown: what makes two refreshes show the same column.
    struct Notice
    {
        Kind kind;
        QString text;
        QString details;

        bool operator==(const Notice& other) const
        {
            return kind == other.kind && text == other.text && details == other.details;
        }
    };

    void refresh();

    /// The notices the open ward needs now, before any is dismissed.
    QList<Notice> currentNotices() const;

    QWidget* makeNotice(const Notice& notice);
    void openReleasePage();
    void dismiss(Kind kind);

    QVBoxLayout* m_layout = nullptr;
    UpdateChecker* m_updates = nullptr;
    QHash<QString, QSet<Kind>> m_dismissed;     ///< by document id
    QList<Notice> m_shown;
};
