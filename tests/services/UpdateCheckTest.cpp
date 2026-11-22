#include "UpdateCheckTest.h"

#include "UpdateCheck.h"

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
