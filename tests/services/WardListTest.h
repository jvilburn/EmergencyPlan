#pragma once

#include <QObject>

class WardListTest : public QObject
{
    Q_OBJECT

public:
    explicit WardListTest(QObject* parent);

private slots:
    void wardNameListsImportedWardsAlphabetically();
    void listNamesEachWardAndMarksWaitingChanges();
};
