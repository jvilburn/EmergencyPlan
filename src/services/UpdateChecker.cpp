#include "UpdateChecker.h"

#include "StrictJson.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

using namespace std::placeholders;

namespace
{

const QLatin1String Repository("jvilburn/EmergencyPlan");
const QLatin1String KeyTagName("tag_name");

/// The version a release tag names: "v1.2.0" and "1.2.0" are both 1.2.0; anything
/// that is not a version is null.
QVersionNumber versionOfTag(const QString& tag)
{
    QString text = tag.trimmed();
    if (text.startsWith('v', Qt::CaseInsensitive))
    {
        text.remove(0, 1);
    }
    qsizetype parsed = 0;
    const QVersionNumber version = QVersionNumber::fromString(text, &parsed);
    return parsed == text.size() && !text.isEmpty() ? version : QVersionNumber();
}

} // namespace

UpdateChecker::UpdateChecker(AppSettings& settings, const QString& runningVersion, Fetch fetch, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_running(versionOfTag(runningVersion))
    , m_fetch(std::move(fetch))
{
}

void UpdateChecker::check(bool force)
{
    const std::optional<AppSettings::UpdateCheck> last = m_settings.updateCheck();
    if (!force && last.has_value() && last->when.secsTo(QDateTime::currentDateTimeUtc()) < CheckIntervalSeconds)
    {
        return;
    }
    m_fetch(QUrl(QString("https://api.github.com/repos/%1/releases/latest").arg(Repository)),
            std::bind(&UpdateChecker::onFetched, this, _1, _2));
}

void UpdateChecker::onFetched(bool ok, const QByteArray& body)
{
    if (!ok)
    {
        return;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    QString tag;
    if (error.error != QJsonParseError::NoError || !document.isObject()
        || !StrictJson::readString(document.object(), KeyTagName, tag))
    {
        return;
    }
    const QVersionNumber latest = versionOfTag(tag);
    if (latest.isNull())
    {
        return;
    }

    const QString before = newerVersion();
    m_settings.setUpdateCheck(AppSettings::UpdateCheck{QDateTime::currentDateTimeUtc(), latest.toString()});
    if (newerVersion() != before)
    {
        emit changed();
    }
}

QString UpdateChecker::newerVersion() const
{
    const std::optional<AppSettings::UpdateCheck> last = m_settings.updateCheck();
    if (!last.has_value())
    {
        return QString();
    }
    const QVersionNumber latest = QVersionNumber::fromString(last->latestVersion);
    if (latest.isNull() || latest <= m_running || last->latestVersion == m_settings.dismissedUpdate())
    {
        return QString();
    }
    return last->latestVersion;
}

QUrl UpdateChecker::releasePage()
{
    return QUrl(QString("https://github.com/%1/releases/latest").arg(Repository));
}

void UpdateChecker::dismiss()
{
    const QString version = newerVersion();
    if (version.isEmpty())
    {
        return;
    }
    m_settings.setDismissedUpdate(version);
    emit changed();
}
