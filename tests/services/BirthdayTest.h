#pragma once

#include <QObject>

class BirthdayTest : public QObject
{
    Q_OBJECT

public:
    explicit BirthdayTest(QObject* parent);

private slots:
    void displayShowsWhicheverPartsAreSet_data();
    void displayShowsWhicheverPartsAreSet();
};
