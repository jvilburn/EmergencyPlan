#include "FamilyEditPanelTest.h"
#include "Coordinates.h"
#include "Document.h"
#include "FamilyChange.h"
#include "FamilyEditPanel.h"
#include "MemberEditor.h"
#include "TestClient.h"
#include "TestWard.h"

#include <QLineEdit>
#include <QPushButton>
#include <QTest>
#include <QTextEdit>

namespace
{

const Coordinates ElmPosition(40.2338, -111.6585);
const Coordinates OakPosition(40.2410, -111.6520);

QList<PersonId> idsOf(const Family& family)
{
    QList<PersonId> ids;
    for (const Person& member : family.members())
    {
        ids.append(member.id());
    }
    return ids;
}

/// The panel's button with that text, or null when it has none.
QPushButton* buttonWithText(const FamilyEditPanel& panel, const QString& text)
{
    for (QPushButton* button : panel.findChildren<QPushButton*>())
    {
        if (button->text() == text)
        {
            return button;
        }
    }
    return nullptr;
}

/// The panel's editor for a member the opened family lacks, or null when there is none.
MemberEditor* addedMemberEditor(const FamilyEditPanel& panel)
{
    QList<PersonId> openedIds = idsOf(panel.openedFamily());
    for (MemberEditor* editor : panel.findChildren<MemberEditor*>())
    {
        if (!openedIds.contains(editor->person().id()))
        {
            return editor;
        }
    }
    return nullptr;
}

/// The widget's line edit with that placeholder text, or null when it has none.
QLineEdit* lineEditWithPlaceholder(const QWidget& widget, const QString& placeholder)
{
    for (QLineEdit* edit : widget.findChildren<QLineEdit*>())
    {
        if (edit->placeholderText() == placeholder)
        {
            return edit;
        }
    }
    return nullptr;
}

/// The panel's address field, or null when it has none.
QTextEdit* addressField(const FamilyEditPanel& panel)
{
    for (QTextEdit* edit : panel.findChildren<QTextEdit*>())
    {
        if (edit->placeholderText() == "Enter address (one line per row)")
        {
            return edit;
        }
    }
    return nullptr;
}

/// The test ward with an entry for the Smith address.
Document smithMapped(const Coordinates& coordinates)
{
    return TestWard::withCoordinates(TestWard::document(), TestWard::smith().address(), coordinates);
}

/// Adds a member through the panel's + Add button, checks it starts last, and types its
/// birth year. Sets added to the new member's id.
void addMemberBornIn(FamilyEditPanel& panel, const QString& birthYear, PersonId& added)
{
    QPushButton* add = buttonWithText(panel, "+ Add");
    QVERIFY(add != nullptr);
    add->click();

    MemberEditor* editor = addedMemberEditor(panel);
    QVERIFY(editor != nullptr);
    added = editor->person().id();
    QCOMPARE(idsOf(panel.family()).last(), added);

    QLineEdit* year = lineEditWithPlaceholder(*editor, "blank = adult");
    QVERIFY(year != nullptr);
    year->setText(birthYear);
}

}  // namespace

FamilyEditPanelTest::FamilyEditPanelTest(QObject* parent)
    : QObject(parent)
{
}

void FamilyEditPanelTest::addedChildMovesIntoPlaceWhenItsBirthYearIsTyped()
{
    Family opened = TestWard::lee(2010);
    TestClient client("Laptop");
    FamilyEditPanel panel(client.session().automaticUpdates(), nullptr);
    panel.setFamily(opened, Document());

    PersonId added;
    addMemberBornIn(panel, "2008", added);
    if (QTest::currentTestFailed())
    {
        return;
    }

    // Ken, Mia, Ed (2005), the added child (2008), Fay (2010).
    QList<PersonId> expected = idsOf(opened);
    expected.insert(3, added);
    Family saved = panel.family();
    QCOMPARE(idsOf(saved), expected);

    std::optional<FamilyChange> change = FamilyChange::between(opened, saved);
    QVERIFY(change.has_value());
    QCOMPARE(idsOf(change->appliedTo(opened)), expected);
}

void FamilyEditPanelTest::addedChildStaysLastWhileAMemberHasNoBirthYear()
{
    Family opened = TestWard::lee(std::nullopt);
    TestClient client("Laptop");
    FamilyEditPanel panel(client.session().automaticUpdates(), nullptr);
    panel.setFamily(opened, Document());

    PersonId added;
    addMemberBornIn(panel, "2008", added);
    if (QTest::currentTestFailed())
    {
        return;
    }

    QList<PersonId> expected = idsOf(opened);
    expected.append(added);
    QCOMPARE(idsOf(panel.family()), expected);
}

void FamilyEditPanelTest::coordinateFieldsAreEnabledOnlyWhileTheAddressIsNotEmpty()
{
    Family unaddressed = TestWard::smith();
    unaddressed.setAddress(Address());
    TestClient client("Laptop");
    FamilyEditPanel panel(client.session().automaticUpdates(), nullptr);
    panel.setFamily(unaddressed, TestWard::document());
    QLineEdit* latitude = lineEditWithPlaceholder(panel, "Latitude");
    QLineEdit* longitude = lineEditWithPlaceholder(panel, "Longitude");
    QTextEdit* address = addressField(panel);
    QPushButton* lookUp = buttonWithText(panel, "Look Up Coordinates");
    QVERIFY(latitude != nullptr);
    QVERIFY(longitude != nullptr);
    QVERIFY(address != nullptr);
    QVERIFY(lookUp != nullptr);

    QVERIFY(!latitude->isEnabled());
    QVERIFY(!longitude->isEnabled());
    QVERIFY(!lookUp->isEnabled());

    address->setPlainText("12 Elm St\nProvo, UT");
    QVERIFY(latitude->isEnabled());
    QVERIFY(longitude->isEnabled());
    QVERIFY(lookUp->isEnabled());

    address->clear();
    QVERIFY(!latitude->isEnabled());
    QVERIFY(!longitude->isEnabled());
    QVERIFY(!lookUp->isEnabled());

    panel.setFamily(TestWard::smith(), TestWard::document());
    QVERIFY(latitude->isEnabled());
    QVERIFY(longitude->isEnabled());
    QVERIFY(lookUp->isEnabled());
}

void FamilyEditPanelTest::coordinatesShownFromTheEntryAreNotTyped()
{
    TestClient client("Laptop");
    FamilyEditPanel panel(client.session().automaticUpdates(), nullptr);
    panel.setFamily(TestWard::smith(), smithMapped(Coordinates(40.12345678, -111.98765432)));
    QLineEdit* latitude = lineEditWithPlaceholder(panel, "Latitude");
    QLineEdit* longitude = lineEditWithPlaceholder(panel, "Longitude");
    QVERIFY(latitude != nullptr);
    QVERIFY(longitude != nullptr);

    QCOMPARE(latitude->text(), QString("40.123457"));
    QCOMPARE(longitude->text(), QString("-111.987654"));
    QVERIFY(!panel.typedCoordinates().has_value());
    QVERIFY(!panel.isDirty());

    // A character typed, then deleted again.
    latitude->end(false);
    latitude->insert("9");
    QVERIFY(panel.typedCoordinates() == Coordinates(40.1234579, -111.987654));
    QVERIFY(panel.isDirty());
    latitude->backspace();

    QCOMPARE(latitude->text(), QString("40.123457"));
    QVERIFY(!panel.typedCoordinates().has_value());
    QVERIFY(!panel.isDirty());
}

void FamilyEditPanelTest::typedCoordinatesAreReturnedAndMakeThePanelDirty()
{
    TestClient client("Laptop");
    FamilyEditPanel panel(client.session().automaticUpdates(), nullptr);
    panel.setFamily(TestWard::smith(), TestWard::document());
    QLineEdit* latitude = lineEditWithPlaceholder(panel, "Latitude");
    QLineEdit* longitude = lineEditWithPlaceholder(panel, "Longitude");
    QVERIFY(latitude != nullptr);
    QVERIFY(longitude != nullptr);
    QVERIFY(latitude->text().isEmpty());
    QVERIFY(longitude->text().isEmpty());
    QVERIFY(!panel.typedCoordinates().has_value());
    QVERIFY(!panel.isDirty());

    latitude->setText("40.2339");
    QVERIFY(!panel.typedCoordinates().has_value());
    longitude->setText("-111.6587");

    QVERIFY(panel.typedCoordinates() == Coordinates(40.2339, -111.6587));
    QVERIFY(panel.isDirty());
    QVERIFY(panel.family() == TestWard::smith());

    // Text other than what the panel showed that reads as the entry is typed, and not dirty.
    FamilyEditPanel mapped(client.session().automaticUpdates(), nullptr);
    mapped.setFamily(TestWard::smith(), smithMapped(ElmPosition));
    QLineEdit* mappedLatitude = lineEditWithPlaceholder(mapped, "Latitude");
    QVERIFY(mappedLatitude != nullptr);
    QCOMPARE(mappedLatitude->text(), QString("40.233800"));
    mappedLatitude->setText("40.2338");

    QVERIFY(mapped.typedCoordinates() == ElmPosition);
    QVERIFY(!mapped.isDirty());
}

void FamilyEditPanelTest::saveIsDisabledWhileTypedCoordinatesDoNotParse()
{
    QString prompt("Enter a latitude and a longitude");
    TestClient client("Laptop");
    FamilyEditPanel panel(client.session().automaticUpdates(), nullptr);
    panel.setFamily(TestWard::smith(), smithMapped(ElmPosition));
    QLineEdit* latitude = lineEditWithPlaceholder(panel, "Latitude");
    QLineEdit* longitude = lineEditWithPlaceholder(panel, "Longitude");
    QPushButton* save = buttonWithText(panel, "Save");
    QVERIFY(latitude != nullptr);
    QVERIFY(longitude != nullptr);
    QVERIFY(save != nullptr);
    QVERIFY(save->isEnabled());
    QVERIFY(save->toolTip().isEmpty());

    longitude->clear();
    QVERIFY(!save->isEnabled());
    QCOMPARE(save->toolTip(), prompt);

    longitude->setText("-111.7");
    QVERIFY(save->isEnabled());
    QVERIFY(save->toolTip().isEmpty());

    latitude->setText("north");
    QVERIFY(!save->isEnabled());
    QCOMPARE(save->toolTip(), prompt);

    latitude->setText("40.3");
    QVERIFY(save->isEnabled());
    QVERIFY(save->toolTip().isEmpty());
    QVERIFY(panel.typedCoordinates() == Coordinates(40.3, -111.7));
}

void FamilyEditPanelTest::refreshShowsTheNewEntryUnlessCoordinatesAreTyped()
{
    Document document = TestWard::document();

    // Untyped fields show the entry for the address shown, once there is one.
    TestClient client("Laptop");
    FamilyEditPanel untyped(client.session().automaticUpdates(), nullptr);
    untyped.setFamily(TestWard::smith(), document);
    QLineEdit* latitude = lineEditWithPlaceholder(untyped, "Latitude");
    QLineEdit* longitude = lineEditWithPlaceholder(untyped, "Longitude");
    QVERIFY(latitude != nullptr);
    QVERIFY(longitude != nullptr);
    QVERIFY(latitude->text().isEmpty());

    untyped.refreshCoordinates(TestWard::withCoordinates(document, TestWard::jones().address(), OakPosition));
    QVERIFY(latitude->text().isEmpty());
    QVERIFY(longitude->text().isEmpty());

    untyped.refreshCoordinates(smithMapped(ElmPosition));
    QCOMPARE(latitude->text(), QString("40.233800"));
    QCOMPARE(longitude->text(), QString("-111.658500"));
    QVERIFY(!untyped.typedCoordinates().has_value());
    QVERIFY(!untyped.isDirty());

    // The address shown, not the one opened.
    FamilyEditPanel readdressed(client.session().automaticUpdates(), nullptr);
    readdressed.setFamily(TestWard::smith(), document);
    QTextEdit* address = addressField(readdressed);
    QLineEdit* readdressedLatitude = lineEditWithPlaceholder(readdressed, "Latitude");
    QLineEdit* readdressedLongitude = lineEditWithPlaceholder(readdressed, "Longitude");
    QVERIFY(address != nullptr);
    QVERIFY(readdressedLatitude != nullptr);
    QVERIFY(readdressedLongitude != nullptr);
    address->setPlainText("4 Oak Ave\nProvo, UT");
    Document bothMapped = TestWard::withCoordinates(smithMapped(ElmPosition), TestWard::jones().address(), OakPosition);
    readdressed.refreshCoordinates(bothMapped);
    QCOMPARE(readdressedLatitude->text(), QString("40.241000"));
    QCOMPARE(readdressedLongitude->text(), QString("-111.652000"));

    // Typed fields are left as typed.
    FamilyEditPanel typed(client.session().automaticUpdates(), nullptr);
    typed.setFamily(TestWard::smith(), document);
    QLineEdit* typedLatitude = lineEditWithPlaceholder(typed, "Latitude");
    QLineEdit* typedLongitude = lineEditWithPlaceholder(typed, "Longitude");
    QVERIFY(typedLatitude != nullptr);
    QVERIFY(typedLongitude != nullptr);
    typedLatitude->setText("40.3");
    typedLongitude->setText("-111.7");

    typed.refreshCoordinates(smithMapped(ElmPosition));

    QCOMPARE(typedLatitude->text(), QString("40.3"));
    QCOMPARE(typedLongitude->text(), QString("-111.7"));
    QVERIFY(typed.typedCoordinates() == Coordinates(40.3, -111.7));
    QVERIFY(typed.isDirty());
}

void FamilyEditPanelTest::lookUpFillsTheFieldsOnlyWhileTheAddressLookedUpIsShown()
{
    TestClient client("Laptop");
    FamilyEditPanel panel(client.session().automaticUpdates(), nullptr);
    panel.setFamily(TestWard::smith(), TestWard::document());
    QLineEdit* latitude = lineEditWithPlaceholder(panel, "Latitude");
    QLineEdit* longitude = lineEditWithPlaceholder(panel, "Longitude");
    QTextEdit* address = addressField(panel);
    QPushButton* lookUp = buttonWithText(panel, "Look Up Coordinates");
    QVERIFY(latitude != nullptr);
    QVERIFY(longitude != nullptr);
    QVERIFY(address != nullptr);
    QVERIFY(lookUp != nullptr);
    QVERIFY(lookUp->isEnabled());
    QString smith = TestWard::smith().address().full();
    QString oak = TestWard::address("4 Oak Ave", "Provo, UT").full();

    lookUp->click();
    QCOMPARE(client.geocoder().requests(), QStringList({smith}));
    QVERIFY(!lookUp->isEnabled());
    QCOMPARE(lookUp->text(), QString("Looking up..."));

    // The address changes before the answer: the answer is not for the address shown.
    address->setPlainText("4 Oak Ave\nProvo, UT");
    QVERIFY(lookUp->isEnabled());
    QCOMPARE(lookUp->text(), QString("Look Up Coordinates"));
    client.geocoder().resolve(smith, ElmPosition);
    QCOMPARE(latitude->text(), QString());
    QCOMPARE(longitude->text(), QString());

    lookUp->click();
    QCOMPARE(client.geocoder().requests(), QStringList({smith, oak}));
    client.geocoder().resolve(oak, OakPosition);
    QVERIFY(lookUp->isEnabled());
    QCOMPARE(lookUp->text(), QString("Look Up Coordinates"));
    QCOMPARE(latitude->text(), QString("40.241000"));
    QCOMPARE(longitude->text(), QString("-111.652000"));
    QVERIFY(panel.typedCoordinates() == OakPosition);
}

void FamilyEditPanelTest::typedFamilyNoteIsReturnedTrimmedAndMakesThePanelDirty()
{
    Family noted = TestWard::smith();
    noted.setNote("Gate code is 1234");
    TestClient client("Laptop");
    FamilyEditPanel panel(client.session().automaticUpdates(), nullptr);
    panel.setFamily(noted, TestWard::document());
    QTextEdit* note = nullptr;
    for (QTextEdit* edit : panel.findChildren<QTextEdit*>())
    {
        if (edit->placeholderText() == "Anything worth noting about this family")
        {
            note = edit;
        }
    }
    QVERIFY(note != nullptr);
    QCOMPARE(note->toPlainText(), QString("Gate code is 1234"));
    QVERIFY(!panel.isDirty());

    note->setPlainText("  Gate code is 5678\n");

    QCOMPARE(panel.family().note(), QString("Gate code is 5678"));
    QVERIFY(panel.isDirty());
    Family expected = noted;
    expected.setNote("Gate code is 5678");
    QVERIFY(panel.family() == expected);

    note->setPlainText("Gate code is 1234");

    QVERIFY(panel.family() == noted);
    QVERIFY(!panel.isDirty());
}
