#include "UpdateFamilyCommandTest.h"
#include "FamilyCommands.h"
#include "Document.h"
#include "TestWard.h"

#include <QTest>

namespace
{

std::unique_ptr<UpdateFamilyCommand> bobPhoneEdit(const QString& phone)
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

Person dan()
{
    return Person::createWithId(PersonId::fromString("person-dan"), Name("Smith", "Dan"),
                                Gender::Male, Birthday(), Phone(), Phone(), QString(),
                                QStringList(), false, QString(), QString());
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

    std::unique_ptr<UpdateFamilyCommand> command = UpdateFamilyCommand::fromEdit(opened, saved);

    QCOMPARE(command->description().heading(), QString("Smith family"));
    QCOMPARE(command->description().lines(), QStringList({"Bob, phone: 555-2222",
                                                          "Alice, email: (none)",
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
    std::unique_ptr<UpdateFamilyCommand> command = UpdateFamilyCommand::fromEdit(opened, saved);
    Document document = TestWard::withFamily(TestWard::document(),
                                             TestWard::withPhone(opened, TestWard::bobId(), "555-9999"));
    Document before = document;

    QVERIFY(command->execute(document) == CommandOutcome::Conflicted);

    QVERIFY(document == before);
    QCOMPARE(TestWard::family(document, TestWard::smithId()).address().full(), QString("12 Elm St, Provo, UT"));
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
    std::unique_ptr<UpdateFamilyCommand> bobEdit = bobPhoneEdit("555-2222");
    std::unique_ptr<UpdateFamilyCommand> aliceEdit = UpdateFamilyCommand::fromEdit(opened, TestWard::withMember(opened, alice));

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
    std::unique_ptr<UpdateFamilyCommand> removal = UpdateFamilyCommand::fromEdit(TestWard::smith(), smithWithout(TestWard::aliceId()));
    Document document = TestWard::withFamily(TestWard::document(),
                                             TestWard::withPhone(TestWard::smith(), TestWard::aliceId(), "555-7777"));

    QVERIFY(removal->execute(document) == CommandOutcome::Conflicted);

    QCOMPARE(QString(TestWard::person(document, TestWard::aliceId()).phone()), QString("555-7777"));
}

void UpdateFamilyCommandTest::memberRemovalAppliesOntoAnUnchangedMemberAndItsInverseRestoresThem()
{
    std::unique_ptr<UpdateFamilyCommand> removal = UpdateFamilyCommand::fromEdit(TestWard::smith(), smithWithout(TestWard::aliceId()));
    Document document = TestWard::document();

    QVERIFY(removal->execute(document) == CommandOutcome::Applied);
    QVERIFY(!document.findPersonById(TestWard::aliceId()).has_value());
    QCOMPARE(TestWard::family(document, TestWard::smithId()).members().size(), qsizetype(1));

    QVERIFY(removal->inverse()->execute(document) == CommandOutcome::Applied);
    QVERIFY(TestWard::person(document, TestWard::aliceId()) == TestWard::alice());
    QCOMPARE(TestWard::family(document, TestWard::smithId()).members().size(), qsizetype(2));
}

void UpdateFamilyCommandTest::memberAdditionHoldsWhenThatPersonIsAlreadyThere()
{
    Family withDan = TestWard::smith();
    withDan.setMembers({TestWard::bob(), TestWard::alice(), dan()});
    std::unique_ptr<UpdateFamilyCommand> addition = UpdateFamilyCommand::fromEdit(TestWard::smith(), withDan);
    Document document = TestWard::withFamily(TestWard::document(), withDan);
    Document before = document;

    QVERIFY(addition->execute(document) == CommandOutcome::Applied);

    QVERIFY(document == before);
    QCOMPARE(TestWard::family(document, TestWard::smithId()).members().size(), qsizetype(3));
}

void UpdateFamilyCommandTest::inverseSwapsTheChangeUnderAnUndoHeading()
{
    std::unique_ptr<UpdateFamilyCommand> edit = bobPhoneEdit("555-2222");
    CommandPtr inverse = edit->inverse();

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
