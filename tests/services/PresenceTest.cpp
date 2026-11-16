#include "PresenceTest.h"

#include "AccessCommands.h"
#include "Presence.h"
#include "TestClient.h"
#include "TestSharing.h"

#include <QTest>

PresenceTest::PresenceTest(QObject* parent)
    : QObject(parent)
{
}

void PresenceTest::othersNamesEachKindAndAUserOnce()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient bobsPhone("Bob's phone");
    TestClient stranger("Stranger");
    TestSharing::share({&alice}, TestSharing::accessOf({&alice, &bob}, {}));
    // Bob has two devices, and there is one invite for a user and one for a device.
    AccessList access = alice.session().document().access();
    AccessUser bobUser = TestSharing::userNamed(alice, "Bob");
    access.setUser(AccessUser(bobUser.id(), bobUser.details(), bobUser.role(), {bob.device(), bobsPhone.device()}));
    Document withPhone = alice.session().document();
    withPhone.setAccess(access);
    alice.session().replayOnto(withPhone);
    QByteArray forCarol = alice.session().invite(InvitedUser{AccessRole::Member, "Carol"}).invite;
    QByteArray forADevice = alice.session().invite(std::nullopt).invite;
    QVERIFY(!forCarol.isEmpty() && !forADevice.isEmpty());

    QSet<QByteArray> present{bob.identity().fingerprint(), bobsPhone.identity().fingerprint(), forCarol,
                             forADevice, stranger.identity().fingerprint()};
    QStringList others = Presence::others(alice.session().document().access(), present);

    QCOMPARE(others, (QStringList{"Bob", "a device invitation", "a device that just joined",
                                  "someone with Carol's invitation"}));
    QVERIFY(Presence::others(alice.session().document().access(), {}).isEmpty());
}

void PresenceTest::lineJoinsNames()
{
    QCOMPARE(Presence::line({}), QString());
    QCOMPARE(Presence::line({"Alice Smith"}), QString("Alice Smith is also here"));
    QCOMPARE(Presence::line({"Alice Smith", "Bob Jones"}), QString("Alice Smith and Bob Jones are also here"));
    QCOMPARE(Presence::line({"Alice Smith", "Bob Jones", "Carol Lee"}),
             QString("Alice Smith, Bob Jones and Carol Lee are also here"));
}
