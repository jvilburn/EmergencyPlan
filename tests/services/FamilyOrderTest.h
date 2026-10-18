#pragma once

#include <QObject>

class FamilyOrderTest : public QObject
{
    Q_OBJECT

public:
    explicit FamilyOrderTest(QObject* parent);

private slots:
    void childGoesAmongTheChildrenOldestToYoungest();
    void childYoungerThanEveryoneGoesAtTheEnd();
    void fatherGoesAfterTheFathersWhateverHisAge();
    void motherGoesAfterTheMothers();
    void otherParentGoesAfterTheMothersAndBeforeTheChildren();
    void endWhenAMembersBirthYearIsUnknown();
    void endWhenThePersonsBirthYearIsUnknown();
};
