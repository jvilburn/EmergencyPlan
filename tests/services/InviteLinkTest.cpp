#include "InviteLinkTest.h"

#include "DocumentCrypto.h"
#include "DocumentLibrary.h"
#include "InviteLink.h"

#include <QtTest>

namespace
{

/// Bytes 0, 1, ... 31: a seed anyone can reproduce.
QByteArray countingSeed()
{
    QByteArray seed;
    for (int i = 0; i < InviteLink::SeedSize; ++i)
    {
        seed.append(char(i));
    }
    return seed;
}

const QString VectorId = "00112233445566778899aabbccddeeff";
const QString VectorLink = "wardplanning:join/00112233445566778899aabbccddeeff/AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8";

} // namespace

void InviteLinkTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void InviteLinkTest::anInviteLinkRoundTripsThroughItsText()
{
    const InviteLink link{VectorId, countingSeed()};
    QCOMPARE(link.toString(), VectorLink);
    QCOMPARE(link.toString().size(), 94);

    const std::optional<InviteLink> parsed = InviteLink::parse(VectorLink);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->documentId, VectorId);
    QCOMPARE(parsed->seed, countingSeed());

    // A browser's trailing slash, and a scheme in capitals, are the same link.
    QCOMPARE(InviteLink::parse(VectorLink + "/")->seed, countingSeed());
    QCOMPARE(InviteLink::parse("WardPlanning:Join/" + VectorLink.mid(18))->documentId, VectorId);

    // Any seed survives the trip, including those whose base64 uses '-' and '_'.
    for (int i = 0; i < 20; ++i)
    {
        const InviteLink random{DocumentLibrary::newDocumentId(), DocumentCrypto::randomBytes(InviteLink::SeedSize)};
        const std::optional<InviteLink> back = InviteLink::parse(random.toString());
        QVERIFY(back.has_value());
        QCOMPARE(back->documentId, random.documentId);
        QCOMPARE(back->seed, random.seed);
    }
}

void InviteLinkTest::aMalformedInviteLinkIsNotOne_data()
{
    const QString seed = VectorLink.mid(VectorLink.lastIndexOf('/') + 1);
    const QString prefix = "wardplanning:join/";

    QTest::addColumn<QString>("argument");
    QTest::newRow("a short seed") << prefix + VectorId + "/" + seed.left(40);
    QTest::newRow("a long seed") << prefix + VectorId + "/" + seed + "AAAA";
    QTest::newRow("a seed with a character outside base64url") << prefix + VectorId + "/" + seed.left(42) + "+";
    QTest::newRow("no seed") << prefix + VectorId;
    QTest::newRow("an empty seed") << prefix + VectorId + "/";
    QTest::newRow("a short id") << prefix + VectorId.left(30) + "/" + seed;
    QTest::newRow("an id in capitals") << prefix + VectorId.toUpper() + "/" + seed;
    QTest::newRow("a path in the id") << prefix + "../" + VectorId + "/" + seed;
    QTest::newRow("a part too many") << prefix + VectorId + "/" + seed + "/more";
    QTest::newRow("the open link no build makes") << "wardplanning:open/" + VectorId;
    QTest::newRow("a web address") << "https://example.test/join/" + VectorId + "/" + seed;
    QTest::newRow("a file") << "C:/wards/" + VectorId + ".wardplanning";
}

void InviteLinkTest::aMalformedInviteLinkIsNotOne()
{
    QFETCH(QString, argument);
    QVERIFY(!InviteLink::parse(argument).has_value());

    // Anchor: the vector itself parses, so the refusal is the change's doing.
    QVERIFY(InviteLink::parse(VectorLink).has_value());
}

void InviteLinkTest::theInviteMessageSaysWhoWhereAndHow()
{
    const InviteLink link{VectorId, countingSeed()};
    QCOMPARE(link.message("Alice Smith", "Tanglewood Ward"),
             QString("Alice Smith has invited you to Tanglewood Ward in Ward Planning. Install it from "
                     "https://github.com/jvilburn/EmergencyPlan/releases/latest, then open this link:\n")
                 + VectorLink);
}
