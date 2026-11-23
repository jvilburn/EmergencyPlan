#pragma once

#include "Server.h"

#include <QObject>
#include <optional>

/// Keeps what a read answered, heard through a queued connection as the window hears it.
class ReadListener : public QObject
{
    Q_OBJECT

public:
    explicit ReadListener(QObject* parent);

    const std::optional<ServerRead>& heard() const { return m_heard; }

public slots:
    void onAnswered(const ServerRead& read);

private:
    std::optional<ServerRead> m_heard;
};

/// The update check offers the release the server lists for a platform exactly when that
/// release's number is above the running app's.
class UpdateCheckTest : public QObject
{
    Q_OBJECT

public:
    explicit UpdateCheckTest(QObject* parent);

private slots:
    void aReleaseWithAHigherNumberIsOffered();
    void aReleaseThatIsNotLaterIsNotOffered();
    void aListWithoutAReleaseForThePlatformOffersNothing();
    void aReleaseListedWithoutItsVersionOrItsInstallerIsNotOffered();
    void theServersOwnListIsThisBuildsRelease();
    void theServersListReachesAListenerOnTheEventLoop();
};
