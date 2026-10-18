#pragma once

#include <QObject>

/// The member editor hands back the person it was given, changed only where the user
/// changed a widget.
class MemberEditorTest : public QObject
{
    Q_OBJECT

public:
    explicit MemberEditorTest(QObject* parent);

private slots:
    void phoneEditKeepsEveryOtherField();
    void untouchedFormReturnsThePersonAsGiven();
    void untouchedFormKeepsEachPartialBirthday();
};
