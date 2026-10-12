#include "RosterDiffTest.h"

#include "DocumentCrypto.h"
#include "RosterDiff.h"
#include "SealingSet.h"

#include <QtTest>

namespace
{

DeviceKeys newKeys()
{
    return DeviceKeys{DocumentCrypto::generateKeypair().publicKey, DocumentCrypto::generateSigningKeypair().verifyKey};
}

MemberId newMember()
{
    MemberId member;
    member.bytes = DocumentCrypto::randomBytes(MemberId::Size);
    return member;
}

Invite newInvite(Invite::For purpose, const MemberId& person, const QString& label)
{
    Invite invite;
    invite.purpose = purpose;
    invite.maker = person;
    invite.label = label;
    invite.keys = newKeys();
    return invite;
}

/// Alice, an admin on her laptop, and Bob, a member on his phone.
struct Ward
{
    MemberId alice = newMember();
    MemberId bob = newMember();
    SealingSet roster;

    Ward()
    {
        roster = SealingSet::of(alice, "Alice", NamedDevice{"Laptop", newKeys()});
        const Invite forBob = newInvite(Invite::For::User, alice, "for Bob");
        roster.addInvite(forBob);
        roster.join(forBob.fingerprint(), NamedDevice{"Phone", newKeys()}, bob, "Bob");
    }
};

} // namespace

void RosterDiffTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void RosterDiffTest::theSameRosterSaysNothing()
{
    const Ward ward;
    QVERIFY(RosterDiff::describe(ward.roster, ward.roster).isEmpty());

    // A rotation's former key is history, not a change in who has access.
    SealingSet rotated = ward.roster;
    rotated.addFormerKey(DocumentCrypto::generateDataKey());
    QVERIFY(RosterDiff::describe(ward.roster, rotated).isEmpty());

    // Anchor: a change in the same pair is seen.
    rotated.setRole(ward.bob, Role::Admin);
    QCOMPARE(RosterDiff::describe(ward.roster, rotated).size(), 1);
}

void RosterDiffTest::aJoinSaysWhoAsWhatAndThatTheInviteWentWithIt()
{
    const Ward ward;
    SealingSet invited = ward.roster;
    const Invite forCarol = newInvite(Invite::For::User, ward.alice, "for Carol");
    QVERIFY(invited.addInvite(forCarol));
    SealingSet joined = invited;
    QVERIFY(joined.join(forCarol.fingerprint(), NamedDevice{"Tablet", newKeys()}, newMember(), "Carol"));

    QCOMPARE(RosterDiff::describe(invited, joined),
             (QStringList{"Carol joined, as a member", "The invite for Carol was used or withdrawn"}));
}

void RosterDiffTest::aRemovalSaysWhoNoLongerHasAccess()
{
    const Ward ward;
    SealingSet removed = ward.roster;
    QVERIFY(removed.removePerson(ward.bob));
    QCOMPARE(RosterDiff::describe(ward.roster, removed), QStringList{"Bob no longer has access"});
}

void RosterDiffTest::aRoleChangeSaysTheNewRole()
{
    const Ward ward;
    SealingSet promoted = ward.roster;
    QVERIFY(promoted.setRole(ward.bob, Role::Admin));
    QCOMPARE(RosterDiff::describe(ward.roster, promoted), QStringList{"Bob is now an admin"});

    SealingSet stepped = promoted;
    QVERIFY(stepped.setRole(ward.alice, Role::Member));
    QCOMPARE(RosterDiff::describe(promoted, stepped), QStringList{"Alice is now a member"});
}

void RosterDiffTest::aDeviceAddedOrUnlinkedNamesBoth()
{
    const Ward ward;
    SealingSet added = ward.roster;
    const Invite forTablet = newInvite(Invite::For::Device, ward.bob, "for Tablet");
    QVERIFY(added.addInvite(forTablet));
    const NamedDevice tablet{"Tablet", newKeys()};
    QVERIFY(added.join(forTablet.fingerprint(), tablet, ward.bob, "Bob"));
    SealingSet invited = ward.roster;
    QVERIFY(invited.addInvite(forTablet));

    QCOMPARE(RosterDiff::describe(invited, added),
             (QStringList{"Bob added a device, Tablet", "The invite for Tablet was used or withdrawn"}));

    SealingSet unlinked = added;
    QVERIFY(unlinked.removeDevice(tablet.fingerprint()));
    QCOMPARE(RosterDiff::describe(added, unlinked), QStringList{"Bob unlinked a device, Tablet"});
}

void RosterDiffTest::anInviteMadeNamesItsMakerAndLabel()
{
    const Ward ward;
    SealingSet invited = ward.roster;
    QVERIFY(invited.addInvite(newInvite(Invite::For::User, ward.bob, "for Dave")));
    QCOMPARE(RosterDiff::describe(ward.roster, invited), QStringList{"Bob made an invite, for Dave"});

    // A device invite reads the same, under its own label.
    SealingSet forDevice = ward.roster;
    QVERIFY(forDevice.addInvite(newInvite(Invite::For::Device, ward.alice, "for Desktop")));
    QCOMPARE(RosterDiff::describe(ward.roster, forDevice), QStringList{"Alice made an invite, for Desktop"});
}

void RosterDiffTest::aWithdrawnInviteSaysSo()
{
    const Ward ward;
    SealingSet invited = ward.roster;
    const Invite forDave = newInvite(Invite::For::User, ward.alice, "for Dave");
    QVERIFY(invited.addInvite(forDave));
    SealingSet withdrawn = invited;
    QVERIFY(withdrawn.removeInvite(forDave.fingerprint()));

    QCOMPARE(RosterDiff::describe(invited, withdrawn), QStringList{"The invite for Dave was used or withdrawn"});
}

void RosterDiffTest::severalChangesComeInOrder()
{
    // People as they were, then those who are new, then invites gone, then invites made.
    const Ward ward;
    SealingSet previous = ward.roster;
    const Invite forCarol = newInvite(Invite::For::User, ward.alice, "for Carol");
    QVERIFY(previous.addInvite(forCarol));

    SealingSet current = previous;
    QVERIFY(current.join(forCarol.fingerprint(), NamedDevice{"Tablet", newKeys()}, newMember(), "Carol"));
    QVERIFY(current.removePerson(ward.bob));
    QVERIFY(current.addInvite(newInvite(Invite::For::Device, ward.alice, "for Desktop")));

    QCOMPARE(RosterDiff::describe(previous, current),
             (QStringList{"Bob no longer has access",
                          "Carol joined, as a member",
                          "The invite for Carol was used or withdrawn",
                          "Alice made an invite, for Desktop"}));
}
