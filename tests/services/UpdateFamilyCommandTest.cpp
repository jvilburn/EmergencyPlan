#include "UpdateFamilyCommandTest.h"
#include "FamilyCommands.h"
#include "Document.h"
#include "TestWard.h"

#include <QJsonArray>
#include <QTest>

namespace
{

UndoableCommandPtr bobPhoneEdit(const QString& phone)
{
    Family opened = TestWard::smith();
    return UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, TestWard::bobId(), phone));
}

Family smithWithout(const PersonId& id)
{
    QList<Person> members;
    for (const Person& member : TestWard::smith().members())
    {
        if (member.id() != id)
        {
            members.append(member);
        }
    }
    Family family = TestWard::smith();
    family.setMembers(members);
    return family;
}

/// The Smith family with one of Bob's fields, or the address, at a value that is neither
/// its value in TestWard::smith() nor in TestWard::smithWithEveryFieldChanged().
Family smithWithThirdValue(const QString& field)
{
    Family family = TestWard::smith();
    if (field == "address")
    {
        family.setAddress(TestWard::address("16 Elm St", "Provo, UT"));
        return family;
    }

    Person bob = TestWard::bob();
    if (field == "name")
    {
        bob.setName(Name("Smith", "Bobby"));
    }
    else if (field == "gender")
    {
        bob.setGender(Gender::Female);
    }
    else if (field == "phone")
    {
        bob.setPhone(Phone("555-9999"));
    }
    else if (field == "altPhone")
    {
        bob.setAltPhone(Phone("555-9998"));
    }
    else if (field == "email")
    {
        bob.setEmail("bobby@example.com");
    }
    else if (field == "birthday")
    {
        bob.setBirthday(Birthday::create(1990, 1, 1));
    }
    else if (field == "callings")
    {
        bob.setCallings({"Ward Clerk"});
    }
    else if (field == "wardUnitNumber")
    {
        bob.setWardUnitNumber("999999");
    }
    else if (field == "stakeUnitNumber")
    {
        bob.setStakeUnitNumber("888888");
    }
    else if (field == "specialNeedNote")
    {
        bob.setSpecialNeedNote("Hard of hearing");
    }
    return TestWard::withMember(family, bob);
}

/// family with Gus inserted at index.
Family withGusAt(const Family& family, qsizetype index)
{
    QList<Person> members = family.members();
    members.insert(index, TestWard::gus());
    Family saved = family;
    saved.setMembers(members);
    return saved;
}

Document documentWith(const Family& family)
{
    Document document;
    document.addFamily(family);
    document.onDocumentChanged(DocumentChange::full());
    return document;
}

QStringList memberIds(const Family& family)
{
    QStringList ids;
    for (const Person& member : family.members())
    {
        ids.append(member.id().toString());
    }
    return ids;
}

qsizetype addedIndex(const Command& command)
{
    QJsonArray added = command.toJson()["change"].toObject()["added"].toArray();
    return added[0].toObject()["index"].toInteger();
}

}  // namespace

UpdateFamilyCommandTest::UpdateFamilyCommandTest(QObject* parent)
    : QObject(parent)
{
}

void UpdateFamilyCommandTest::editHoldsOnlyTheFieldsThatDiffer()
{
    QJsonObject change = bobPhoneEdit("555-2222")->toJson()["change"].toObject();

    QCOMPARE(change.keys(), QStringList({"members"}));
    QJsonObject members = change["members"].toObject();
    QCOMPARE(members.keys(), QStringList({TestWard::bobId().toString()}));
    QJsonObject bob = members[TestWard::bobId().toString()].toObject();
    QCOMPARE(bob.keys(), QStringList({"phone"}));
    QCOMPARE(bob["phone"].toObject()["before"].toString(), QString("555-1111"));
    QCOMPARE(bob["phone"].toObject()["after"].toString(), QString("555-2222"));
}

void UpdateFamilyCommandTest::editThatChangesNothingIsNoCommand()
{
    QVERIFY(UpdateFamilyCommand::fromEdit(TestWard::smith(), TestWard::smith()) == nullptr);
    QVERIFY(bobPhoneEdit("555-2222") != nullptr);
}

void UpdateFamilyCommandTest::descriptionIsAHeadingAndALinePerValueSet()
{
    Family opened = TestWard::smith();
    Person alice = TestWard::alice();
    alice.setEmail(QString());
    Family saved = TestWard::withMember(TestWard::withPhone(opened, TestWard::bobId(), "555-2222"), alice);
    saved.setAddress(TestWard::address("14 Elm St", "Provo, UT"));

    UndoableCommandPtr command = UpdateFamilyCommand::fromEdit(opened, saved);

    QCOMPARE(command->description().heading(), QString("Smith family"));
    QCOMPARE(command->description().lines(), QStringList({"Bob, phone: 555-2222",
                                                          "Alice, email: (none)",
                                                          "address: 14 Elm St, Provo, UT"}));
}

void UpdateFamilyCommandTest::everyFieldEditDescribesEachField()
{
    UndoableCommandPtr command = UpdateFamilyCommand::fromEdit(TestWard::smith(), TestWard::smithWithEveryFieldChanged());

    QCOMPARE(command->description().lines(),
             QStringList({"Bob, name: Smith, Robert",
                          "Bob, gender: (none)",
                          "Bob, phone: 555-2222",
                          "Bob, other phone: 555-2223",
                          "Bob, email: robert@example.com",
                          "Bob, birthday: Jul 4, 1980",
                          "Bob, callings: Clerk",
                          "Bob, not a parent",
                          "Bob, ward unit: 123456",
                          "Bob, stake unit: 654321",
                          "Bob, special need: Uses a wheelchair; the ramp is at the back door.",
                          "added: Smith, Dan",
                          "removed: Smith, Alice",
                          "address: 14 Elm St, Provo, UT"}));
}

void UpdateFamilyCommandTest::appliesWhereItsStartConditionHolds()
{
    Document document = TestWard::document();

    QVERIFY(bobPhoneEdit("555-2222")->execute(document) == CommandOutcome::Applied);

    QCOMPARE(QString(TestWard::person(document, TestWard::bobId()).phone()), QString("555-2222"));
    QVERIFY(TestWard::person(document, TestWard::aliceId()) == TestWard::alice());
    QVERIFY(TestWard::family(document, TestWard::jonesId()) == TestWard::jones());
}

void UpdateFamilyCommandTest::appliesAndChangesNothingWhereItsResultHolds()
{
    Document document = TestWard::withFamily(TestWard::document(),
                                             TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-2222"));
    Document before = document;

    QVERIFY(bobPhoneEdit("555-2222")->execute(document) == CommandOutcome::Applied);

    QVERIFY(document == before);
}

void UpdateFamilyCommandTest::conflictsAndTouchesNothingWhereAFieldMoved()
{
    Family opened = TestWard::smith();
    Family saved = TestWard::withPhone(opened, TestWard::bobId(), "555-2222");
    saved.setAddress(TestWard::address("14 Elm St", "Provo, UT"));
    UndoableCommandPtr command = UpdateFamilyCommand::fromEdit(opened, saved);
    Document document = TestWard::withFamily(TestWard::document(),
                                             TestWard::withPhone(opened, TestWard::bobId(), "555-9999"));
    Document before = document;

    QVERIFY(command->execute(document) == CommandOutcome::Conflicted);

    QVERIFY(document == before);
    QCOMPARE(TestWard::family(document, TestWard::smithId()).address().full(), QString("12 Elm St, Provo, UT"));
}

void UpdateFamilyCommandTest::everyFieldMovedToAThirdValueConflicts_data()
{
    QTest::addColumn<QString>("field");
    // isParent has two values, so it has no third value to move to.
    const QStringList fields = {"name", "gender", "phone", "altPhone", "email", "birthday", "callings",
                                "wardUnitNumber", "stakeUnitNumber", "specialNeedNote", "address"};
    for (const QString& field : fields)
    {
        QTest::newRow(qPrintable(field)) << field;
    }
}

void UpdateFamilyCommandTest::everyFieldMovedToAThirdValueConflicts()
{
    QFETCH(QString, field);
    UndoableCommandPtr command = UpdateFamilyCommand::fromEdit(TestWard::smith(), TestWard::smithWithEveryFieldChanged());
    Document document = TestWard::withFamily(TestWard::document(), smithWithThirdValue(field));
    QVERIFY(document != TestWard::document());
    Document before = document;

    QVERIFY(command->execute(document) == CommandOutcome::Conflicted);

    QVERIFY(document == before);
}

void UpdateFamilyCommandTest::conflictsWhereTheFamilyIsGone()
{
    Document document = TestWard::document();
    document.removeFamily(TestWard::smithId());
    Document before = document;

    QVERIFY(bobPhoneEdit("555-2222")->execute(document) == CommandOutcome::Conflicted);

    QVERIFY(document == before);
}

void UpdateFamilyCommandTest::conflictsWhereAChangedMemberIsGone()
{
    Document document = TestWard::withFamily(TestWard::document(), smithWithout(TestWard::bobId()));
    Document before = document;

    QVERIFY(bobPhoneEdit("555-2222")->execute(document) == CommandOutcome::Conflicted);

    QVERIFY(document == before);
}

void UpdateFamilyCommandTest::editsToDifferentMembersBothApply()
{
    Family opened = TestWard::smith();
    Person alice = TestWard::alice();
    alice.setEmail("alice@new.example.com");
    UndoableCommandPtr bobEdit = bobPhoneEdit("555-2222");
    UndoableCommandPtr aliceEdit = UpdateFamilyCommand::fromEdit(opened, TestWard::withMember(opened, alice));

    Document bobFirst = TestWard::document();
    QVERIFY(bobEdit->execute(bobFirst) == CommandOutcome::Applied);
    QVERIFY(aliceEdit->execute(bobFirst) == CommandOutcome::Applied);
    Document aliceFirst = TestWard::document();
    QVERIFY(aliceEdit->execute(aliceFirst) == CommandOutcome::Applied);
    QVERIFY(bobEdit->execute(aliceFirst) == CommandOutcome::Applied);

    QCOMPARE(QString(TestWard::person(bobFirst, TestWard::bobId()).phone()), QString("555-2222"));
    QCOMPARE(TestWard::person(bobFirst, TestWard::aliceId()).email(), QString("alice@new.example.com"));
    QVERIFY(bobFirst == aliceFirst);
}

void UpdateFamilyCommandTest::memberRemovalConflictsAfterThatMemberChanged()
{
    UndoableCommandPtr removal = UpdateFamilyCommand::fromEdit(TestWard::smith(), smithWithout(TestWard::aliceId()));
    Document document = TestWard::withFamily(TestWard::document(),
                                             TestWard::withPhone(TestWard::smith(), TestWard::aliceId(), "555-7777"));

    QVERIFY(removal->execute(document) == CommandOutcome::Conflicted);

    QCOMPARE(QString(TestWard::person(document, TestWard::aliceId()).phone()), QString("555-7777"));
}

void UpdateFamilyCommandTest::memberRemovalAppliesOntoAnUnchangedMemberAndItsInverseRestoresThem()
{
    UndoableCommandPtr removal = UpdateFamilyCommand::fromEdit(TestWard::smith(), smithWithout(TestWard::aliceId()));
    Document document = TestWard::document();

    QVERIFY(removal->execute(document) == CommandOutcome::Applied);
    QVERIFY(!document.findPersonById(TestWard::aliceId()).has_value());
    QCOMPARE(TestWard::family(document, TestWard::smithId()).members().size(), qsizetype(1));

    QVERIFY(removal->inverse()->execute(document) == CommandOutcome::Applied);
    QVERIFY(TestWard::person(document, TestWard::aliceId()) == TestWard::alice());
    QVERIFY(TestWard::family(document, TestWard::smithId()) == TestWard::smith());
}

void UpdateFamilyCommandTest::memberAdditionHoldsWhenThatPersonIsAlreadyThere()
{
    Family withDan = TestWard::smith();
    withDan.setMembers({TestWard::bob(), TestWard::alice(), TestWard::dan()});
    UndoableCommandPtr addition = UpdateFamilyCommand::fromEdit(TestWard::smith(), withDan);
    Document document = TestWard::withFamily(TestWard::document(), withDan);
    Document before = document;

    QVERIFY(addition->execute(document) == CommandOutcome::Applied);

    QVERIFY(document == before);
    QCOMPARE(TestWard::family(document, TestWard::smithId()).members().size(), qsizetype(3));
}

void UpdateFamilyCommandTest::addedMemberTakesItsIndexInTheSavedFamily()
{
    // Every birth year is known, and the member order rule would put Gus at 3, after Ed.
    Family opened = TestWard::lee(2010);
    QCOMPARE(opened.insertionIndexFor(TestWard::gus()), qsizetype(3));
    UndoableCommandPtr addition = UpdateFamilyCommand::fromEdit(opened, withGusAt(opened, 1));
    Document document = documentWith(opened);

    QCOMPARE(addedIndex(*addition), qsizetype(1));
    QVERIFY(addition->execute(document) == CommandOutcome::Applied);

    QCOMPARE(memberIds(TestWard::family(document, opened.id())),
             QStringList({"person-ken", "person-gus", "person-mia", "person-ed", "person-fay"}));
}

void UpdateFamilyCommandTest::inverseSwapsTheChangeUnderAnUndoHeading()
{
    UndoableCommandPtr edit = bobPhoneEdit("555-2222");
    UndoableCommandPtr inverse = edit->inverse();

    QCOMPARE(inverse->description().heading(), QString("Undo: Smith family"));
    QCOMPARE(inverse->description().lines(), edit->description().lines());

    Document document = TestWard::document();
    QVERIFY(edit->execute(document) == CommandOutcome::Applied);
    QVERIFY(inverse->execute(document) == CommandOutcome::Applied);
    QVERIFY(document == TestWard::document());

    Document moved = TestWard::withFamily(TestWard::document(),
                                          TestWard::withPhone(TestWard::smith(), TestWard::bobId(), "555-9999"));
    Document movedBefore = moved;
    QVERIFY(inverse->execute(moved) == CommandOutcome::Conflicted);
    QVERIFY(moved == movedBefore);
}

void UpdateFamilyCommandTest::inverseOfTheEveryFieldEditRestoresTheDocumentExactly()
{
    UndoableCommandPtr edit = UpdateFamilyCommand::fromEdit(TestWard::smith(), TestWard::smithWithEveryFieldChanged());
    Document document = TestWard::document();

    QVERIFY(edit->execute(document) == CommandOutcome::Applied);
    QVERIFY(TestWard::family(document, TestWard::smithId()) == TestWard::smithWithEveryFieldChanged());

    QVERIFY(edit->inverse()->execute(document) == CommandOutcome::Applied);
    QVERIFY(TestWard::family(document, TestWard::smithId()) == TestWard::smith());
    QVERIFY(document == TestWard::document());
}
