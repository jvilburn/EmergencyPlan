#include "FamilyEditPanelTest.h"
#include "FamilyChange.h"
#include "FamilyEditPanel.h"
#include "MemberEditor.h"
#include "TestWard.h"

#include <QLineEdit>
#include <QPushButton>
#include <QTest>

namespace
{

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
    FamilyEditPanel panel(nullptr);
    panel.setFamily(opened);

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
    FamilyEditPanel panel(nullptr);
    panel.setFamily(opened);

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
