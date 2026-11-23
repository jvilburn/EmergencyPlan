#include "UpdateCheckTest.h"

#include "FakeServer.h"
#include "UpdateCheck.h"

#include <QCoreApplication>
#include <QFile>
#include <QTest>

namespace
{
/// The server's list as server/src/releases.json gives it, with the Windows release numbered number.
QByteArray listWith(int number)
{
    return QString(R"({"windows":{"name":"Windows","version":"2.0.0-beta.5","number":%1,)"
                   R"("url":"https://example.test/releases/WardPlanning-2.0.0-beta.5-setup.exe"}})")
        .arg(number)
        .toUtf8();
}
}  // namespace

ReadListener::ReadListener(QObject* parent)
    : QObject(parent)
{
}

void ReadListener::onAnswered(const ServerRead& read)
{
    m_heard = read;
}

UpdateCheckTest::UpdateCheckTest(QObject* parent)
    : QObject(parent)
{
}

void UpdateCheckTest::aReleaseWithAHigherNumberIsOffered()
{
    std::optional<Release> release = UpdateCheck::later(listWith(5), "windows", 4);

    QVERIFY(release.has_value());
    QCOMPARE(release->version, QString("2.0.0-beta.5"));
    QCOMPARE(release->number, 5);
    QCOMPARE(release->installer, QUrl("https://example.test/releases/WardPlanning-2.0.0-beta.5-setup.exe"));
    // The numbers alone order releases: 10 is above 9, as it would not be as text.
    QVERIFY(UpdateCheck::later(listWith(10), "windows", 9).has_value());
}

void UpdateCheckTest::aReleaseThatIsNotLaterIsNotOffered()
{
    // The same release, and an earlier one the server was pointed back at.
    QVERIFY(!UpdateCheck::later(listWith(5), "windows", 5).has_value());
    QVERIFY(!UpdateCheck::later(listWith(5), "windows", 6).has_value());
    QVERIFY(!UpdateCheck::later(listWith(9), "windows", 10).has_value());
}

void UpdateCheckTest::aListWithoutAReleaseForThePlatformOffersNothing()
{
    QVERIFY(UpdateCheck::later(listWith(5), "windows", 4).has_value());

    QVERIFY(!UpdateCheck::later(listWith(5), "macos", 4).has_value());
    QVERIFY(!UpdateCheck::later("{}", "windows", 4).has_value());
    QVERIFY(!UpdateCheck::later("not a list", "windows", 4).has_value());
    // A release listed without a number is not ordered against the running app.
    QByteArray unnumbered = R"({"windows":{"name":"Windows","version":"2.0.0-beta.5","url":"https://example.test/a.exe"}})";
    QVERIFY(!UpdateCheck::later(unnumbered, "windows", 4).has_value());
}

void UpdateCheckTest::aReleaseListedWithoutItsVersionOrItsInstallerIsNotOffered()
{
    QByteArray whole = R"({"windows":{"version":"2.0.2","number":6,"url":"https://example.test/a.exe"}})";
    QVERIFY(UpdateCheck::later(whole, "windows", 5).has_value());

    QVERIFY(!UpdateCheck::later(R"({"windows":{"number":6,"url":"https://example.test/a.exe"}})", "windows", 5).has_value());
    QVERIFY(!UpdateCheck::later(R"({"windows":{"version":"2.0.2","number":6}})", "windows", 5).has_value());
}

void UpdateCheckTest::theServersOwnListIsThisBuildsRelease()
{
    // The build takes its version and number from the list the server serves, so the app reads
    // its own release out of that file, offers it to the release before, and not to itself.
    QFile file(WARD_PLANNING_RELEASES_FILE);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray list = file.readAll();

    std::optional<Release> release = UpdateCheck::later(list, "windows", WARD_PLANNING_RELEASE_NUMBER - 1);
    QVERIFY(release.has_value());
    QCOMPARE(release->version, QString(WARD_PLANNING_VERSION));
    QCOMPARE(release->number, WARD_PLANNING_RELEASE_NUMBER);
    QCOMPARE(release->installer.scheme(), QString("https"));
    QVERIFY(release->installer.path().endsWith(QString("/WardPlanning-%1-setup.exe").arg(WARD_PLANNING_VERSION)));
    QVERIFY(!UpdateCheck::later(list, "windows", WARD_PLANNING_RELEASE_NUMBER).has_value());
}

void UpdateCheckTest::theServersListReachesAListenerOnTheEventLoop()
{
    FakeServer transport;
    transport.setReleases(listWith(5));
    Server server(&transport);
    ReadListener listener(nullptr);

    ServerReading* reading = server.releases(&listener);
    connect(reading, &ServerReading::answered, &listener, &ReadListener::onAnswered, Qt::QueuedConnection);
    transport.settle();
    // The answer waits for the event loop.
    QVERIFY(!listener.heard().has_value());
    QCoreApplication::processEvents();

    QVERIFY(listener.heard().has_value());
    QVERIFY(listener.heard()->status == ServerReadStatus::Found);
    QCOMPARE(listener.heard()->bytes, listWith(5));
}
