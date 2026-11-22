#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <optional>

/// A release of the app, as the server lists it for one platform.
struct Release
{
    QString version;  ///< as people read it, such as 2.0.0-beta.5
    int number;       ///< counts the releases: each release's is above the one before it
    QUrl installer;   ///< where its installer is downloaded from
};

/// Reads the server's list of current releases (GET /releases/latest) for a release later than
/// the running app. Releases are ordered by their numbers alone; the version is for people.
class UpdateCheck
{
public:
    /// The key of this platform's release in the server's list, such as windows.
    static QString platform();

    /// The release list gives for platform, when its number is above runningNumber; otherwise
    /// none, as when the list has no release for the platform.
    static std::optional<Release> later(const QByteArray& list, const QString& platform, int runningNumber);
};
