#pragma once

#include <QObject>

/// The family edit panel places an added member by the member order rule, again as its
/// details are filled in, and saves the family in that order.
class FamilyEditPanelTest : public QObject
{
    Q_OBJECT

public:
    explicit FamilyEditPanelTest(QObject* parent);

private slots:
    void addedChildMovesIntoPlaceWhenItsBirthYearIsTyped();
    void addedChildStaysLastWhileAMemberHasNoBirthYear();
};
