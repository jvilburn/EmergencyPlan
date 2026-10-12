#include "UpdateCheckerTest.h"

#include "AppSettings.h"
#include "UpdateChecker.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace
{

/// GitHub as the test scripts it: each check gets the next answer, and the urls asked
/// are kept.
struct FakeGitHub
{
    QList<std::pair<bool, QByteArray>> answers;
    QStringList asked;

    UpdateChecker::Fetch fetch()
    {
        return [this](const QUrl& url, UpdateChecker::Fetched done) {
            asked.append(url.toString());
            const std::pair<bool, QByteArray> answer = answers.isEmpty() ? std::make_pair(false, QByteArray()) : answers.takeFirst();
            done(answer.first, answer.second);
        };
    }

    static QByteArray release(const QString& tag)
    {
        return QByteArray("{\"tag_name\": \"") + tag.toUtf8() + "\", \"html_url\": \"https://example.test/r\"}";
    }
};

struct Checker
{
    Checker(const QString& running, FakeGitHub& github)
        : settings(AppSettings::open(folder.filePath("settings.json"), nullptr))
        , checker(*settings, running, github.fetch(), nullptr)
    {
    }

    QTemporaryDir folder;
    std::unique_ptr<AppSettings> settings;
    UpdateChecker checker;
};

} // namespace

void UpdateCheckerTest::aNewerReleaseIsReported()
{
    FakeGitHub github;
    github.answers.append({true, FakeGitHub::release("v1.3.0")});
    Checker app("1.2.0", github);
    QSignalSpy changed(&app.checker, &UpdateChecker::changed);

    QVERIFY(app.checker.newerVersion().isEmpty());
    app.checker.check(false);
    QCOMPARE(github.asked, QStringList{"https://api.github.com/repos/jvilburn/EmergencyPlan/releases/latest"});
    QCOMPARE(changed.count(), 1);
    QCOMPARE(app.checker.newerVersion(), QString("1.3.0"));
    QCOMPARE(UpdateChecker::releasePage().toString(), QString("https://github.com/jvilburn/EmergencyPlan/releases/latest"));

    // The answer is kept, so a fresh checker on the same settings knows it without asking.
    UpdateChecker again(*app.settings, "1.2.0", github.fetch(), nullptr);
    QCOMPARE(again.newerVersion(), QString("1.3.0"));
    QCOMPARE(github.asked.size(), 1);
}

void UpdateCheckerTest::anEqualOrOlderReleaseIsNot()
{
    for (const char* tag : {"v1.2.0", "1.2.0", "v1.1.9", "v0.9.0"})
    {
        FakeGitHub github;
        github.answers.append({true, FakeGitHub::release(tag)});
        Checker app("1.2.0", github);
        QSignalSpy changed(&app.checker, &UpdateChecker::changed);
        app.checker.check(false);
        QCOMPARE(github.asked.size(), 1);
        QCOMPARE(changed.count(), 0);
        QVERIFY2(app.checker.newerVersion().isEmpty(), tag);
        QCOMPARE(app.settings->updateCheck()->latestVersion, QString(tag).remove(0, QString(tag).startsWith('v') ? 1 : 0));
    }
}

void UpdateCheckerTest::versionsCompareNumerically()
{
    FakeGitHub github;
    github.answers.append({true, FakeGitHub::release("v1.10.0")});
    Checker app("1.9.0", github);
    app.checker.check(false);
    QCOMPARE(app.checker.newerVersion(), QString("1.10.0"));
}

void UpdateCheckerTest::aFailedOrMalformedAnswerIsSilent()
{
    FakeGitHub github;
    github.answers.append({false, QByteArray()});
    github.answers.append({true, QByteArray("not json")});
    github.answers.append({true, QByteArray("{\"tag_name\": \"release-candidate\"}")});
    github.answers.append({true, QByteArray("[]")});
    Checker app("1.2.0", github);
    QSignalSpy changed(&app.checker, &UpdateChecker::changed);

    for (int i = 0; i < 4; ++i)
    {
        app.checker.check(true);
    }
    QCOMPARE(github.asked.size(), 4);
    QCOMPARE(changed.count(), 0);
    QVERIFY(app.checker.newerVersion().isEmpty());
    QVERIFY(!app.settings->updateCheck().has_value());
}

void UpdateCheckerTest::aCheckWithinADayIsSkipped()
{
    FakeGitHub github;
    github.answers.append({true, FakeGitHub::release("v1.3.0")});
    github.answers.append({true, FakeGitHub::release("v1.4.0")});
    Checker app("1.2.0", github);

    app.checker.check(false);
    QCOMPARE(github.asked.size(), 1);
    QCOMPARE(app.checker.newerVersion(), QString("1.3.0"));

    // The same day: nothing asked, the known answer stands. Forced: asked.
    app.checker.check(false);
    QCOMPARE(github.asked.size(), 1);
    QCOMPARE(app.checker.newerVersion(), QString("1.3.0"));
    app.checker.check(true);
    QCOMPARE(github.asked.size(), 2);
    QCOMPARE(app.checker.newerVersion(), QString("1.4.0"));

    // A day later: asked again.
    github.answers.append({true, FakeGitHub::release("v1.5.0")});
    app.settings->setUpdateCheck(AppSettings::UpdateCheck{
        QDateTime::currentDateTimeUtc().addSecs(-UpdateChecker::CheckIntervalSeconds - 1), "1.4.0"});
    app.checker.check(false);
    QCOMPARE(github.asked.size(), 3);
    QCOMPARE(app.checker.newerVersion(), QString("1.5.0"));
}

void UpdateCheckerTest::notNowKeepsThatVersionQuietUntilANewerOne()
{
    FakeGitHub github;
    github.answers.append({true, FakeGitHub::release("v1.2.0")});
    github.answers.append({true, FakeGitHub::release("v1.2.0")});
    github.answers.append({true, FakeGitHub::release("v1.3.0")});
    Checker app("1.1.0", github);
    QSignalSpy changed(&app.checker, &UpdateChecker::changed);

    app.checker.check(true);
    QCOMPARE(app.checker.newerVersion(), QString("1.2.0"));
    QCOMPARE(changed.count(), 1);

    app.checker.dismiss();
    QCOMPARE(changed.count(), 2);
    QVERIFY(app.checker.newerVersion().isEmpty());
    QCOMPARE(app.settings->dismissedUpdate(), QString("1.2.0"));

    // 1.2.0 again: still quiet. 1.3.0: shows.
    app.checker.check(true);
    QVERIFY(app.checker.newerVersion().isEmpty());
    QCOMPARE(changed.count(), 2);
    app.checker.check(true);
    QCOMPARE(app.checker.newerVersion(), QString("1.3.0"));
    QCOMPARE(changed.count(), 3);

    // Nothing to dismiss once the newer version is shown again and taken: dismiss
    // with nothing newer changes nothing.
    app.checker.dismiss();
    QVERIFY(app.checker.newerVersion().isEmpty());
    app.checker.dismiss();
    QCOMPARE(changed.count(), 4);
}
