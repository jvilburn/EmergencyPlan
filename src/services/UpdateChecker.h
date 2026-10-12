#pragma once

#include "AppSettings.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVersionNumber>

#include <functional>

/// Whether a newer Ward Planning has been published, read from GitHub Releases and so
/// apart from the Ward Planning server. The newest release's tag, less a leading v, is
/// compared with the running version. Silent on any failure: no release reachable, a
/// body that is not a release, a tag that is not a version.
///
/// Asked at launch and then at most once a day, the last answer kept in AppSettings so
/// a known newer version shows between checks. Not now on a version keeps that version
/// quiet; a later one shows again.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    /// One GET: whether the body arrived, and the body.
    using Fetched = std::function<void(bool ok, const QByteArray& body)>;
    using Fetch = std::function<void(const QUrl& url, Fetched done)>;

    static constexpr int CheckIntervalSeconds = 24 * 60 * 60;

    UpdateChecker(AppSettings& settings, const QString& runningVersion, Fetch fetch, QObject* parent);

    /// Asks GitHub for the newest release, unless the last answer is less than a day
    /// old. force asks regardless.
    void check(bool force);

    /// The newest published version when it is newer than this one and not dismissed;
    /// empty otherwise.
    QString newerVersion() const;

    /// Where Download goes.
    static QUrl releasePage();

    /// Not now: the version newerVersion names stays quiet.
    void dismiss();

signals:
    /// newerVersion may read differently.
    void changed();

private:
    void onFetched(bool ok, const QByteArray& body);

    AppSettings& m_settings;
    QVersionNumber m_running;
    Fetch m_fetch;
};
