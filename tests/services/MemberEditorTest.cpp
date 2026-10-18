#include "MemberEditorTest.h"
#include "MemberEditor.h"
#include "TestWard.h"

#include <QLineEdit>
#include <QTest>

namespace
{

/// Bob with a special-need note and both unit numbers, which the form does not show, and
/// a stored name that Name(surname, given) does not reproduce.
Person bobWithFieldsTheFormDoesNotShow()
{
    Person bob = TestWard::bob();
    bob.setName(Name(QString("Smith,Bob")));
    bob.setSpecialNeedNote("Uses a wheelchair");
    bob.setWardUnitNumber("123456");
    bob.setStakeUnitNumber("654321");
    return bob;
}

/// The editor's line edit with that placeholder text, or null when it has none.
QLineEdit* lineEditWithPlaceholder(const MemberEditor& editor, const QString& placeholder)
{
    for (QLineEdit* edit : editor.findChildren<QLineEdit*>())
    {
        if (edit->placeholderText() == placeholder)
        {
            return edit;
        }
    }
    return nullptr;
}

}  // namespace

MemberEditorTest::MemberEditorTest(QObject* parent)
    : QObject(parent)
{
}

void MemberEditorTest::phoneEditKeepsEveryOtherField()
{
    Person original = bobWithFieldsTheFormDoesNotShow();
    QVERIFY(Name(original.surname(), original.givenNames()) != original.name());
    MemberEditor editor(nullptr);
    editor.setPerson(original);

    QLineEdit* phone = lineEditWithPlaceholder(editor, "Phone number");
    QVERIFY(phone != nullptr);
    phone->setText("555-9999");

    Person edited = editor.person();
    QCOMPARE(QString(edited.phone()), QString("555-9999"));
    QCOMPARE(QString(edited.name()), QString("Smith,Bob"));
    QCOMPARE(edited.specialNeedNote(), QString("Uses a wheelchair"));
    QCOMPARE(edited.wardUnitNumber(), QString("123456"));
    QCOMPARE(edited.stakeUnitNumber(), QString("654321"));

    Person expected = original;
    expected.setPhone(Phone("555-9999"));
    QVERIFY(edited == expected);
}

void MemberEditorTest::untouchedFormReturnsThePersonAsGiven()
{
    Person original = bobWithFieldsTheFormDoesNotShow();
    MemberEditor editor(nullptr);
    editor.setPerson(original);

    QVERIFY(editor.person() == original);
}
