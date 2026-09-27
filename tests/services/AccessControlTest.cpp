#include "AccessControlTest.h"

#include "AccessControl.h"
#include "ChainTestSupport.h"
#include "DocumentCrypto.h"

#include <QtTest>

using namespace ChainTestSupport;

namespace
{

const QByteArray Payload = "the ward directory";

/// Alice, Bob, and Carol, one device each. Alice is the only admin unless a test
/// names others, and the writer unless a test says otherwise.
struct Fixture
{
    QString documentId;
    DataKey dataKey;
    TestDevice alice;
    TestDevice bob;
    TestDevice carol;
    DocumentHeader header;
    AccessRoster roster;

    Fingerprint writer() const { return alice.fingerprint(); }
};

/// The people only. A second device needs its person's member id, so tests make
/// the people first and then seal whichever devices they want.
Fixture people()
{
    Fixture f;
    f.documentId = newDocumentId();
    f.dataKey = DocumentCrypto::generateDataKey();
    f.alice = newPerson();
    f.bob = newPerson();
    f.carol = newPerson();
    return f;
}

/// A header and roster that agree, listing these devices and these admins,
/// written by Alice.
void seal(Fixture& f, const QList<TestDevice>& devices, const QList<MemberId>& admins)
{
    f.roster = rosterOf(devices, admins);
    f.header = buildHeader(f.documentId, 1, 1, devices, f.alice, f.dataKey, admins);
}

void seal(Fixture& f, const QList<TestDevice>& devices)
{
    seal(f, devices, {f.alice.member});
}

Fixture fixture()
{
    Fixture f = people();
    seal(f, {f.alice, f.bob, f.carol});
    return f;
}

RosterPerson personFor(const TestDevice& device,
                       const QString& name,
                       RosterPerson::Role role = RosterPerson::Role::Member)
{
    return RosterPerson::create(device.member, name, rosterDevice(device, "Laptop"), role);
}

/// Finishes the change as the save path will, encrypts the payload under it, and
/// returns the ciphertext.
QByteArray writeAs(AccessChange& change, const TestDevice& writer)
{
    finishWrite(change.header, writer);
    return DocumentCrypto::encryptPayload(Payload, change.dataKey, change.header);
}

/// Whether this device can open what was written: unseal its own entry and
/// decrypt the payload with the key it finds there.
bool opens(const DocumentHeader& header, const QByteArray& sealed, const TestDevice& device)
{
    const std::optional<DataKey> key = unsealFor(header, device);
    if (!key.has_value())
    {
        return false;
    }

    const std::optional<QByteArray> opened =
        DocumentCrypto::decryptPayload(sealed, key.value(), header);
    return opened.has_value() && opened.value() == Payload;
}

/// Finishes the change as this writer and checks it links from the header it
/// was built on.
bool linksAs(AccessChange change, const DocumentHeader& previous, const TestDevice& writer)
{
    finishWrite(change.header, writer);
    return DocumentCrypto::verifyLink(previous, change.header);
}

} // namespace

void AccessControlTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

// --- Grant -------------------------------------------------------------------

void AccessControlTest::grantLetsTheNewPersonsDeviceOpenTheDocument()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    const std::optional<AccessChange> result =
        AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), personFor(dave, "Dave Kent"));
    QVERIFY(result.has_value());

    AccessChange change = result.value();
    QVERIFY(change.roster.contains(dave.member));

    const QByteArray sealed = writeAs(change, f.alice);
    QVERIFY(opens(change.header, sealed, dave));
}

void AccessControlTest::grantLeavesTheDataKeyAndGenerationAlone()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    const std::optional<AccessChange> result =
        AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), personFor(dave, "Dave Kent"));
    QVERIFY(result.has_value());

    // A new member may read what came before; that is what being granted access
    // to an existing ward means.
    QVERIFY(result.value().dataKey == f.dataKey);
    QCOMPARE(result.value().header.keyGeneration, f.header.keyGeneration);
    QVERIFY(unsealFor(result.value().header, f.bob) == f.dataKey);
}

void AccessControlTest::grantMakesTheNewPersonAMember()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    // A request carries whatever its sender put in it. Admin is given by an
    // admin, separately, never by asking.
    const std::optional<AccessChange> result = AccessControl::grant(
        f.header, f.roster, f.dataKey, f.writer(), personFor(dave, "Dave Kent", RosterPerson::Role::Admin));
    QVERIFY(result.has_value());

    QVERIFY(!result.value().roster.person(dave.member).value().isAdmin());
    QVERIFY(result.value().header.admins == QList<MemberId>{f.alice.member});
}

void AccessControlTest::grantRefusesAWriterWhoIsNotAnAdmin()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    QVERIFY(!AccessControl::grant(f.header,
                                  f.roster,
                                  f.dataKey,
                                  f.bob.fingerprint(),
                                  personFor(dave, "Dave Kent"))
                 .has_value());
}

void AccessControlTest::grantRefusesAMemberIdAlreadyInTheRoster()
{
    const Fixture f = fixture();
    const TestDevice bobAgain = newDeviceFor(f.bob.member);

    QVERIFY(!AccessControl::grant(f.header,
                                  f.roster,
                                  f.dataKey,
                                  f.writer(),
                                  personFor(bobAgain, "Bob Hale"))
                 .has_value());
}

void AccessControlTest::grantRefusesAPersonWithMoreThanOneDevice()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();
    const TestDevice davePhone = newDeviceFor(dave.member);

    RosterPerson withTwo = personFor(dave, "Dave Kent");
    QVERIFY(withTwo.addDevice(rosterDevice(davePhone, "Phone")));

    // Further devices come by linking, from the person's own device.
    QVERIFY(!AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), withTwo).has_value());
}

void AccessControlTest::grantRefusesADeviceWithNoName()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    // A request carries whatever name its sender typed. One the payload reader
    // would refuse would make the ward unreadable for everyone once granted.
    const RosterPerson unnamed = RosterPerson::create(
        dave.member, "Dave Kent", rosterDevice(dave, ""), RosterPerson::Role::Member);
    QVERIFY(!AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), unnamed).has_value());

    const RosterPerson nameless = RosterPerson::create(
        dave.member, "", rosterDevice(dave, "Laptop"), RosterPerson::Role::Member);
    QVERIFY(!AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), nameless).has_value());
}

void AccessControlTest::grantRefusesADeviceWithInvalidKeys()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    const RosterDevice noKeys = RosterDevice::create("Laptop", PublicKey(), VerifyKey());
    const RosterPerson person =
        RosterPerson::create(dave.member, "Dave Kent", noKeys, RosterPerson::Role::Member);

    QVERIFY(!AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), person).has_value());
}

void AccessControlTest::grantRefusesADeviceSharingAKey()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    // Bob's public key with Dave's own verify key: a different fingerprint, so
    // only the shared-key rule stands in the way.
    const RosterDevice copied =
        RosterDevice::create("Laptop", f.bob.sealing.publicKey, dave.signing.verifyKey);
    const RosterPerson person =
        RosterPerson::create(dave.member, "Dave Kent", copied, RosterPerson::Role::Member);

    QVERIFY(!AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), person).has_value());
}

void AccessControlTest::grantRefusesAnInvalidDataKey()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    QVERIFY(!AccessControl::grant(f.header,
                                  f.roster,
                                  DataKey(),
                                  f.writer(),
                                  personFor(dave, "Dave Kent"))
                 .has_value());
}

void AccessControlTest::grantVerifiesAsALink()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    const std::optional<AccessChange> result =
        AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), personFor(dave, "Dave Kent"));
    QVERIFY(result.has_value());
    QVERIFY(linksAs(result.value(), f.header, f.alice));
}

// --- Link --------------------------------------------------------------------

void AccessControlTest::linkDeviceLetsTheNewDeviceOpenTheDocumentAndLinkVerifies()
{
    const Fixture f = fixture();
    const TestDevice alicePhone = newDeviceFor(f.alice.member);

    const std::optional<AccessChange> result = AccessControl::linkDevice(
        f.header, f.roster, f.dataKey, f.writer(), rosterDevice(alicePhone, "Phone"));
    QVERIFY(result.has_value());

    AccessChange change = result.value();
    QVERIFY(change.dataKey == f.dataKey);

    const QByteArray sealed = writeAs(change, f.alice);
    QVERIFY(opens(change.header, sealed, alicePhone));
    QVERIFY(DocumentCrypto::verifyLink(f.header, change.header));
}

void AccessControlTest::linkDeviceIsOpenToAMember()
{
    const Fixture f = fixture();
    const TestDevice bobPhone = newDeviceFor(f.bob.member);

    // Managing your own devices takes no admin.
    const std::optional<AccessChange> result = AccessControl::linkDevice(
        f.header, f.roster, f.dataKey, f.bob.fingerprint(), rosterDevice(bobPhone, "Phone"));
    QVERIFY(result.has_value());

    QCOMPARE(result.value().roster.person(f.bob.member).value().deviceCount(), 2);
    QVERIFY(linksAs(result.value(), f.header, f.bob));
}

void AccessControlTest::linkDeviceRefusesAnotherPersonsDevice()
{
    const Fixture f = fixture();

    // Alice claiming Bob's device as her own.
    QVERIFY(!AccessControl::linkDevice(f.header,
                                       f.roster,
                                       f.dataKey,
                                       f.writer(),
                                       rosterDevice(f.bob, "Stolen"))
                 .has_value());
}

void AccessControlTest::linkDeviceRefusesADeviceAlreadyLinked()
{
    const Fixture f = fixture();

    // Alice's own device again under a new name. That would be a rename, which
    // is not what linking means.
    QVERIFY(!AccessControl::linkDevice(f.header,
                                       f.roster,
                                       f.dataKey,
                                       f.writer(),
                                       rosterDevice(f.alice, "Renamed"))
                 .has_value());
}

void AccessControlTest::linkDeviceRefusesADeviceWithNoName()
{
    const Fixture f = fixture();
    const TestDevice alicePhone = newDeviceFor(f.alice.member);

    QVERIFY(!AccessControl::linkDevice(f.header,
                                       f.roster,
                                       f.dataKey,
                                       f.writer(),
                                       rosterDevice(alicePhone, ""))
                 .has_value());
}

void AccessControlTest::linkDeviceRefusesADeviceSharingAKey()
{
    const Fixture f = fixture();
    const TestDevice alicePhone = newDeviceFor(f.alice.member);

    const RosterDevice copied =
        RosterDevice::create("Phone", alicePhone.sealing.publicKey, f.carol.signing.verifyKey);

    QVERIFY(!AccessControl::linkDevice(f.header, f.roster, f.dataKey, f.writer(), copied)
                 .has_value());
}

void AccessControlTest::linkDeviceRefusesAWriterNotOnTheRoster()
{
    const Fixture f = fixture();
    const TestDevice stranger = newPerson();
    const TestDevice strangerPhone = newDeviceFor(stranger.member);

    QVERIFY(!AccessControl::linkDevice(f.header,
                                       f.roster,
                                       f.dataKey,
                                       stranger.fingerprint(),
                                       rosterDevice(strangerPhone, "Phone"))
                 .has_value());
}

// --- Unlink ------------------------------------------------------------------

void AccessControlTest::unlinkDeviceRotatesTheKeyAndTheUnlinkedDeviceCannotUnseal()
{
    Fixture f = people();
    const TestDevice aliceLostPhone = newDeviceFor(f.alice.member);
    seal(f, {f.alice, f.bob, f.carol, aliceLostPhone});

    const std::optional<AccessChange> result =
        AccessControl::unlinkDevice(f.header, f.roster, f.writer(), aliceLostPhone.fingerprint());
    QVERIFY(result.has_value());

    AccessChange change = result.value();
    QVERIFY(change.dataKey != f.dataKey);
    QCOMPARE(change.header.keyGeneration, f.header.keyGeneration + 1);

    const QByteArray sealed = writeAs(change, f.alice);
    QVERIFY(!unsealFor(change.header, aliceLostPhone).has_value());
    QVERIFY(opens(change.header, sealed, f.alice));

    // The lost phone still holds the old key. It opens nothing written now.
    QVERIFY(!DocumentCrypto::decryptPayload(sealed, f.dataKey, change.header).has_value());

    QVERIFY(DocumentCrypto::verifyLink(f.header, change.header));
}

void AccessControlTest::unlinkDeviceRefusesTheLastDevice()
{
    const Fixture f = fixture();

    // Alice's only device is necessarily the one writing, so the writer rule
    // refuses this before the last-device rule is reached. Both say the same
    // thing: losing your last device is leaving, not unlinking.
    QVERIFY(!AccessControl::unlinkDevice(f.header, f.roster, f.writer(), f.alice.fingerprint())
                 .has_value());

    // An admin dropping Bob's only device is removing Bob, which is its own
    // operation.
    QVERIFY(!AccessControl::unlinkDevice(f.header, f.roster, f.writer(), f.bob.fingerprint())
                 .has_value());
}

void AccessControlTest::unlinkDeviceLetsAnAdminDropAnotherPersonsDevice()
{
    Fixture f = people();
    const TestDevice bobStolenPhone = newDeviceFor(f.bob.member);
    seal(f, {f.alice, f.bob, f.carol, bobStolenPhone});

    const std::optional<AccessChange> result =
        AccessControl::unlinkDevice(f.header, f.roster, f.writer(), bobStolenPhone.fingerprint());
    QVERIFY(result.has_value());

    AccessChange change = result.value();
    QCOMPARE(change.header.keyGeneration, f.header.keyGeneration + 1);

    const QByteArray sealed = writeAs(change, f.alice);
    QVERIFY(!unsealFor(change.header, bobStolenPhone).has_value());
    QVERIFY(opens(change.header, sealed, f.bob));
    QVERIFY(DocumentCrypto::verifyLink(f.header, change.header));
}

void AccessControlTest::unlinkDeviceRefusesAMemberDroppingAnotherPersonsDevice()
{
    // Alice has two devices, so the last-device rule cannot be what refuses this.
    Fixture f = people();
    const TestDevice aliceSecond = newDeviceFor(f.alice.member);
    seal(f, {f.alice, f.bob, f.carol, aliceSecond});

    QVERIFY(!AccessControl::unlinkDevice(f.header, f.roster, f.bob.fingerprint(), aliceSecond.fingerprint())
                 .has_value());
}

void AccessControlTest::unlinkDeviceRefusesTheWritingDevice()
{
    Fixture f = people();
    const TestDevice aliceSecond = newDeviceFor(f.alice.member);
    seal(f, {f.alice, f.bob, f.carol, aliceSecond});

    // A version's writer is always among its recipients. A device cannot write
    // itself out; another of Alice's devices unlinks it.
    QVERIFY(!AccessControl::unlinkDevice(f.header, f.roster, f.writer(), f.alice.fingerprint())
                 .has_value());

    QVERIFY(AccessControl::unlinkDevice(f.header,
                                        f.roster,
                                        aliceSecond.fingerprint(),
                                        f.alice.fingerprint())
                .has_value());
}

// --- Remove ------------------------------------------------------------------

void AccessControlTest::removeDropsEveryDeviceOfThePerson()
{
    Fixture f = people();
    const TestDevice bobPhone = newDeviceFor(f.bob.member);
    seal(f, {f.alice, f.bob, f.carol, bobPhone});

    const std::optional<AccessChange> result =
        AccessControl::remove(f.header, f.roster, f.writer(), f.bob.member);
    QVERIFY(result.has_value());

    QVERIFY(!result.value().header.entryFor(f.bob.fingerprint()).has_value());
    QVERIFY(!result.value().header.entryFor(bobPhone.fingerprint()).has_value());
    QVERIFY(!result.value().roster.contains(f.bob.member));
    QCOMPARE(result.value().roster.personCount(), 2);
    QCOMPARE(result.value().header.recipients.size(), qsizetype(2));
}

void AccessControlTest::removeRotatesTheKeyAndBumpsTheGeneration()
{
    const Fixture f = fixture();

    const std::optional<AccessChange> result =
        AccessControl::remove(f.header, f.roster, f.writer(), f.bob.member);
    QVERIFY(result.has_value());

    QVERIFY(result.value().dataKey != f.dataKey);
    QCOMPARE(result.value().header.keyGeneration, f.header.keyGeneration + 1);
}

void AccessControlTest::removedPersonCannotOpenWhatComesNext()
{
    // The model in one test: seal to three, remove one, and the removed device
    // finds no entry - and the data key it still holds opens nothing new - while
    // everyone who remains opens it.
    const Fixture f = fixture();

    const std::optional<AccessChange> result =
        AccessControl::remove(f.header, f.roster, f.writer(), f.bob.member);
    QVERIFY(result.has_value());

    AccessChange change = result.value();
    const QByteArray sealed = writeAs(change, f.alice);

    QVERIFY(!unsealFor(change.header, f.bob).has_value());
    QVERIFY(!sealed.isEmpty());
    QVERIFY(!DocumentCrypto::decryptPayload(sealed, f.dataKey, change.header).has_value());

    QVERIFY(opens(change.header, sealed, f.alice));
    QVERIFY(opens(change.header, sealed, f.carol));
}

void AccessControlTest::removeVerifiesAsALink()
{
    const Fixture f = fixture();

    const std::optional<AccessChange> result =
        AccessControl::remove(f.header, f.roster, f.writer(), f.bob.member);
    QVERIFY(result.has_value());
    QVERIFY(linksAs(result.value(), f.header, f.alice));
}

void AccessControlTest::removeLetsAnAdminRemoveAnotherAdmin()
{
    Fixture f = people();
    seal(f, {f.alice, f.bob, f.carol}, canonicalAdmins({f.alice.member, f.carol.member}));

    const std::optional<AccessChange> result =
        AccessControl::remove(f.header, f.roster, f.writer(), f.carol.member);
    QVERIFY(result.has_value());

    QVERIFY(result.value().header.admins == QList<MemberId>{f.alice.member});
    QVERIFY(linksAs(result.value(), f.header, f.alice));
}

void AccessControlTest::removeRefusesAWriterWhoIsNotAnAdmin()
{
    const Fixture f = fixture();

    QVERIFY(!AccessControl::remove(f.header, f.roster, f.bob.fingerprint(), f.carol.member)
                 .has_value());
}

void AccessControlTest::removeRefusesTheWritersOwnPerson()
{
    Fixture f = people();
    const TestDevice aliceSecond = newDeviceFor(f.alice.member);
    seal(f, {f.alice, f.bob, f.carol, aliceSecond});

    // Removing yourself is leaving, from any of your devices.
    QVERIFY(!AccessControl::remove(f.header, f.roster, f.writer(), f.alice.member).has_value());
    QVERIFY(!AccessControl::remove(f.header, f.roster, aliceSecond.fingerprint(), f.alice.member)
                 .has_value());
}

void AccessControlTest::removeRefusesSomeoneNotOnTheRoster()
{
    const Fixture f = fixture();

    QVERIFY(!AccessControl::remove(f.header, f.roster, f.writer(), newMember()).has_value());
}

// --- Roles -------------------------------------------------------------------

void AccessControlTest::setRolePromotesAMemberAndLinkVerifies()
{
    const Fixture f = fixture();

    const std::optional<AccessChange> result = AccessControl::setRole(
        f.header, f.roster, f.dataKey, f.writer(), f.bob.member, RosterPerson::Role::Admin);
    QVERIFY(result.has_value());

    // Who may read is unchanged, so the key is too.
    QVERIFY(result.value().dataKey == f.dataKey);
    QCOMPARE(result.value().header.keyGeneration, f.header.keyGeneration);

    QVERIFY(result.value().roster.person(f.bob.member).value().isAdmin());
    QVERIFY(result.value().header.admins == canonicalAdmins({f.alice.member, f.bob.member}));
    QVERIFY(linksAs(result.value(), f.header, f.alice));
}

void AccessControlTest::setRoleLetsAnAdminStepDownWhenAnotherRemains()
{
    Fixture f = people();
    seal(f, {f.alice, f.bob, f.carol}, canonicalAdmins({f.alice.member, f.bob.member}));

    const std::optional<AccessChange> result = AccessControl::setRole(
        f.header, f.roster, f.dataKey, f.writer(), f.alice.member, RosterPerson::Role::Member);
    QVERIFY(result.has_value());

    QVERIFY(result.value().header.admins == QList<MemberId>{f.bob.member});
    QVERIFY(linksAs(result.value(), f.header, f.alice));
}

void AccessControlTest::setRoleRefusesDemotingTheLastAdmin()
{
    const Fixture f = fixture();

    // A ward with no admin could never grant or remove anyone again.
    QVERIFY(!AccessControl::setRole(
                 f.header, f.roster, f.dataKey, f.writer(), f.alice.member, RosterPerson::Role::Member)
                 .has_value());
}

void AccessControlTest::setRoleRefusesAWriterWhoIsNotAnAdmin()
{
    const Fixture f = fixture();

    QVERIFY(!AccessControl::setRole(f.header,
                                    f.roster,
                                    f.dataKey,
                                    f.bob.fingerprint(),
                                    f.bob.member,
                                    RosterPerson::Role::Admin)
                 .has_value());
}

void AccessControlTest::setRoleRefusesSomeoneNotOnTheRoster()
{
    const Fixture f = fixture();

    QVERIFY(!AccessControl::setRole(
                 f.header, f.roster, f.dataKey, f.writer(), newMember(), RosterPerson::Role::Admin)
                 .has_value());
}

// --- Leave -------------------------------------------------------------------

void AccessControlTest::leaveDropsEveryDeviceOfTheLeaverAndLinkVerifies()
{
    Fixture f = people();
    const TestDevice bobPhone = newDeviceFor(f.bob.member);
    seal(f, {f.alice, f.bob, f.carol, bobPhone});

    const std::optional<AccessChange> result =
        AccessControl::leave(f.header, f.roster, f.dataKey, f.bob.fingerprint());
    QVERIFY(result.has_value());

    AccessChange change = result.value();
    QVERIFY(!change.roster.contains(f.bob.member));
    QVERIFY(!change.header.entryFor(f.bob.fingerprint()).has_value());
    QVERIFY(!change.header.entryFor(bobPhone.fingerprint()).has_value());

    // The one version whose writer is not among its recipients.
    QVERIFY(linksAs(change, f.header, f.bob));
}

void AccessControlTest::leaveDoesNotRotateTheKey()
{
    const Fixture f = fixture();

    // The leaver would generate the new key, and so know it.
    const std::optional<AccessChange> result =
        AccessControl::leave(f.header, f.roster, f.dataKey, f.bob.fingerprint());
    QVERIFY(result.has_value());

    QVERIFY(result.value().dataKey == f.dataKey);
    QCOMPARE(result.value().header.keyGeneration, f.header.keyGeneration);
}

void AccessControlTest::remainingPeopleOpenTheVersionALeaveWrote()
{
    const Fixture f = fixture();

    const std::optional<AccessChange> result =
        AccessControl::leave(f.header, f.roster, f.dataKey, f.bob.fingerprint());
    QVERIFY(result.has_value());

    AccessChange change = result.value();
    const QByteArray sealed = writeAs(change, f.bob);

    QVERIFY(opens(change.header, sealed, f.alice));
    QVERIFY(opens(change.header, sealed, f.carol));
    QVERIFY(!unsealFor(change.header, f.bob).has_value());
}

void AccessControlTest::leaveRefusesTheLastAdminWhileOthersRemain()
{
    const Fixture f = fixture();

    // The last admin names a successor first.
    QVERIFY(!AccessControl::leave(f.header, f.roster, f.dataKey, f.writer()).has_value());
}

void AccessControlTest::leaveLetsAnAdminGoWhenAnotherRemains()
{
    Fixture f = people();
    seal(f, {f.alice, f.bob, f.carol}, canonicalAdmins({f.alice.member, f.carol.member}));

    const std::optional<AccessChange> result =
        AccessControl::leave(f.header, f.roster, f.dataKey, f.writer());
    QVERIFY(result.has_value());

    QVERIFY(result.value().header.admins == QList<MemberId>{f.carol.member});
    QVERIFY(linksAs(result.value(), f.header, f.alice));
}

void AccessControlTest::theLastPersonsLeaveListsNoOne()
{
    Fixture f = people();
    seal(f, {f.alice});

    const std::optional<AccessChange> result =
        AccessControl::leave(f.header, f.roster, f.dataKey, f.writer());
    QVERIFY(result.has_value());

    QVERIFY(result.value().roster.isEmpty());
    QVERIFY(result.value().header.recipients.isEmpty());
    QVERIFY(result.value().header.admins.isEmpty());
    QVERIFY(linksAs(result.value(), f.header, f.alice));
}

void AccessControlTest::leaveRefusesAWriterNotOnTheRoster()
{
    const Fixture f = fixture();
    const TestDevice stranger = newPerson();

    QVERIFY(!AccessControl::leave(f.header, f.roster, f.dataKey, stranger.fingerprint()).has_value());
}

// --- Reseal and match --------------------------------------------------------

void AccessControlTest::resealAllGivesEveryMemberAnEntry()
{
    Fixture f = people();
    const TestDevice carolPhone = newDeviceFor(f.carol.member);
    seal(f, {f.alice, f.bob, f.carol, carolPhone});
    const DataKey fresh = DocumentCrypto::generateDataKey();

    DocumentHeader emptied = f.header;
    emptied.recipients.clear();

    const std::optional<DocumentHeader> resealed = AccessControl::resealAll(emptied, f.roster, fresh);
    QVERIFY(resealed.has_value());

    QCOMPARE(resealed.value().recipients.size(), qsizetype(4));
    for (const TestDevice& device : {f.alice, f.bob, f.carol, carolPhone})
    {
        QVERIFY(unsealFor(resealed.value(), device) == fresh);
    }
}

void AccessControlTest::resealAllCopiesEachMembersVerifyKey()
{
    const Fixture f = fixture();

    // Starting from no entries at all, so a resealAll that left its input alone
    // could not pass.
    DocumentHeader emptied = f.header;
    emptied.recipients.clear();

    const std::optional<DocumentHeader> resealed =
        AccessControl::resealAll(emptied, f.roster, f.dataKey);
    QVERIFY(resealed.has_value());

    for (const TestDevice& device : {f.alice, f.bob, f.carol})
    {
        const std::optional<RecipientEntry> entry = resealed.value().entryFor(device.fingerprint());
        QVERIFY(entry.has_value());
        QVERIFY(entry.value().verifyKey == device.signing.verifyKey);
        QVERIFY(entry.value().publicKey == device.sealing.publicKey);
        QVERIFY(entry.value().member == device.member);
    }
}

void AccessControlTest::resealAllListsTheRostersAdmins()
{
    Fixture f = people();
    const QList<MemberId> admins = canonicalAdmins({f.alice.member, f.carol.member});
    seal(f, {f.alice, f.bob, f.carol}, admins);

    // Starting from no admins, so a resealAll that left the list alone could not
    // pass.
    DocumentHeader emptied = f.header;
    emptied.admins.clear();

    const std::optional<DocumentHeader> resealed =
        AccessControl::resealAll(emptied, f.roster, f.dataKey);
    QVERIFY(resealed.has_value());
    QCOMPARE(resealed.value().admins.size(), qsizetype(2));
    QVERIFY(resealed.value().admins == admins);
}

void AccessControlTest::resealAllRefusesAnInvalidDataKey()
{
    const Fixture f = fixture();

    // Rather than writing an entry nobody can open.
    QVERIFY(!AccessControl::resealAll(f.header, f.roster, DataKey()).has_value());
}

void AccessControlTest::headerEntryWithNoRosterDeviceFailsTheMatch()
{
    const Fixture f = fixture();
    const TestDevice stranger = newPerson();

    const DocumentHeader withStranger = buildHeader(f.documentId,
                                                    1,
                                                    1,
                                                    {f.alice, f.bob, f.carol, stranger},
                                                    f.alice,
                                                    f.dataKey);

    // An unattributable key must never sit on the access list.
    QVERIFY(!AccessControl::headerMatchesRoster(withStranger, f.roster));
}

void AccessControlTest::rosterDeviceWithNoHeaderEntryFailsTheMatch()
{
    const Fixture f = fixture();

    const DocumentHeader missingCarol =
        buildHeader(f.documentId, 1, 1, {f.alice, f.bob}, f.alice, f.dataKey);

    QVERIFY(!AccessControl::headerMatchesRoster(missingCarol, f.roster));
}

void AccessControlTest::entryWhoseVerifyKeyDisagreesWithRosterFailsTheMatch()
{
    const Fixture f = fixture();
    QVERIFY(AccessControl::headerMatchesRoster(f.header, f.roster));

    DocumentHeader altered = f.header;
    for (RecipientEntry& entry : altered.recipients)
    {
        if (entry.fingerprint == f.bob.fingerprint())
        {
            entry.verifyKey = DocumentCrypto::generateSigningKeypair().verifyKey;
        }
    }

    QVERIFY(!AccessControl::headerMatchesRoster(altered, f.roster));
}

void AccessControlTest::entryWhoseMemberDisagreesWithRosterFailsTheMatch()
{
    const Fixture f = fixture();

    // Bob's device filed under Carol: the same keys and fingerprint, so only the
    // member comparison can catch it. It is how one person's device would be
    // passed off as another person's.
    DocumentHeader altered = f.header;
    for (RecipientEntry& entry : altered.recipients)
    {
        if (entry.fingerprint == f.bob.fingerprint())
        {
            entry.member = f.carol.member;
        }
    }

    QVERIFY(!AccessControl::headerMatchesRoster(altered, f.roster));
}

void AccessControlTest::entryWhosePublicKeyDisagreesWithRosterFailsTheMatch()
{
    const Fixture f = fixture();

    // Bob's entry sealing to someone else's key: Bob would find an entry he
    // cannot open.
    DocumentHeader altered = f.header;
    for (RecipientEntry& entry : altered.recipients)
    {
        if (entry.fingerprint == f.bob.fingerprint())
        {
            entry.publicKey = DocumentCrypto::generateKeypair().publicKey;
        }
    }

    QVERIFY(!AccessControl::headerMatchesRoster(altered, f.roster));
}

void AccessControlTest::headerWithADuplicateEntryFailsTheMatch()
{
    const Fixture f = fixture();

    // Four entries for three devices. Without the duplicate check the counts
    // would still agree after the second copy overwrote the first.
    DocumentHeader altered = f.header;
    for (const RecipientEntry& entry : f.header.recipients)
    {
        if (entry.fingerprint == f.bob.fingerprint())
        {
            altered.recipients.append(entry);
        }
    }
    QCOMPARE(altered.recipients.size(), qsizetype(4));

    QVERIFY(!AccessControl::headerMatchesRoster(altered, f.roster));
}

void AccessControlTest::headerWhoseAdminsDisagreeWithRosterFailsTheMatch()
{
    const Fixture f = fixture();
    QVERIFY(AccessControl::headerMatchesRoster(f.header, f.roster));

    // Every entry agrees; only the admin list differs. A header naming an extra
    // admin is how a member would claim rights the roster does not give.
    DocumentHeader extra = f.header;
    extra.admins = canonicalAdmins({f.alice.member, f.bob.member});
    QVERIFY(!AccessControl::headerMatchesRoster(extra, f.roster));

    DocumentHeader none = f.header;
    none.admins.clear();
    QVERIFY(!AccessControl::headerMatchesRoster(none, f.roster));
}

// --- Every operation ---------------------------------------------------------

void AccessControlTest::everyAccessChangeProducesAMatchingHeaderAndRoster()
{
    Fixture f = people();
    const TestDevice aliceSecond = newDeviceFor(f.alice.member);
    seal(f, {f.alice, f.bob, f.carol, aliceSecond});

    const TestDevice dave = newPerson();
    const TestDevice alicePhone = newDeviceFor(f.alice.member);

    struct Case
    {
        std::optional<AccessChange> result;
        TestDevice writer;
    };

    const QList<Case> cases = {
        {AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), personFor(dave, "Dave Kent")),
         f.alice},
        {AccessControl::linkDevice(
             f.header, f.roster, f.dataKey, f.writer(), rosterDevice(alicePhone, "Phone")),
         f.alice},
        {AccessControl::unlinkDevice(f.header, f.roster, f.writer(), aliceSecond.fingerprint()),
         f.alice},
        {AccessControl::remove(f.header, f.roster, f.writer(), f.bob.member), f.alice},
        {AccessControl::setRole(
             f.header, f.roster, f.dataKey, f.writer(), f.carol.member, RosterPerson::Role::Admin),
         f.alice},
        {AccessControl::leave(f.header, f.roster, f.dataKey, f.bob.fingerprint()), f.bob},
    };

    for (const Case& entry : cases)
    {
        QVERIFY(entry.result.has_value());
        QVERIFY(AccessControl::headerMatchesRoster(entry.result.value().header,
                                                   entry.result.value().roster));

        // And each one, finished by exactly its writer, links.
        QVERIFY(linksAs(entry.result.value(), f.header, entry.writer));
    }
}

void AccessControlTest::operationsRefuseAHeaderThatDisagreesWithItsRoster()
{
    // The roster names Dave; the header does not. Everything else about each
    // operation below is valid, so the mismatch is the only thing to refuse.
    Fixture f = people();
    const TestDevice aliceSecond = newDeviceFor(f.alice.member);
    const TestDevice dave = newPerson();
    seal(f, {f.alice, f.bob, f.carol, aliceSecond});

    AccessRoster withDave = f.roster;
    QVERIFY(withDave.addPerson(personFor(dave, "Dave Kent")));
    QVERIFY(!AccessControl::headerMatchesRoster(f.header, withDave));

    const TestDevice eve = newPerson();
    const TestDevice alicePhone = newDeviceFor(f.alice.member);

    QVERIFY(!AccessControl::grant(f.header, withDave, f.dataKey, f.writer(), personFor(eve, "Eve Lund"))
                 .has_value());
    QVERIFY(!AccessControl::linkDevice(
                 f.header, withDave, f.dataKey, f.writer(), rosterDevice(alicePhone, "Phone"))
                 .has_value());
    QVERIFY(!AccessControl::unlinkDevice(f.header, withDave, f.writer(), aliceSecond.fingerprint())
                 .has_value());
    QVERIFY(!AccessControl::remove(f.header, withDave, f.writer(), f.bob.member).has_value());
    QVERIFY(!AccessControl::setRole(
                 f.header, withDave, f.dataKey, f.writer(), f.carol.member, RosterPerson::Role::Admin)
                 .has_value());
    QVERIFY(!AccessControl::leave(f.header, withDave, f.dataKey, f.bob.fingerprint()).has_value());
}

void AccessControlTest::changesClearWhatAWriteMustProduceAfresh()
{
    const Fixture f = fixture();
    const TestDevice dave = newPerson();

    const std::optional<AccessChange> result =
        AccessControl::grant(f.header, f.roster, f.dataKey, f.writer(), personFor(dave, "Dave Kent"));
    QVERIFY(result.has_value());

    const DocumentHeader& header = result.value().header;

    // The key is unchanged on a grant, so carrying the old nonce forward would
    // set up nonce reuse the moment a save path forgot to make a new one.
    QVERIFY(header.nonce.isEmpty());
    QVERIFY(header.payloadHash.isEmpty());
    QVERIFY(header.writerSignature.isEmpty());
    QVERIFY(!header.lastWriteTime.isValid());
    QVERIFY(header.lastWriter == f.writer());

    // And an unfinished change cannot be encrypted by accident.
    QVERIFY(DocumentCrypto::encryptPayload(Payload, result.value().dataKey, header).isEmpty());
}
