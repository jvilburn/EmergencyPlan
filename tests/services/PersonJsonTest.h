#pragma once

#include <QObject>

class PersonJsonTest : public QObject
{
    Q_OBJECT

public:
    explicit PersonJsonTest(QObject* parent);

private slots:
    void yearOnlyBirthdayRoundTrips();
    void monthAndDayBirthdayRoundTrips();
    void fullBirthdayRoundTrips();
    void emptyBirthdayRoundTrips();
};
