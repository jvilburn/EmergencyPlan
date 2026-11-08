#pragma once

#include "WardPayload.h"

#include <QJsonObject>
#include <QObject>

/// Every reader reads exactly what its toJson writes, checked over every object, field and
/// value of a local file's payload with a value in every field of every part.
class StrictReadersTest : public QObject
{
    Q_OBJECT

public:
    explicit StrictReadersTest(QObject* parent);

private slots:
    void initTestCase();

    void everyFieldRoundTrips();
    void everyUnexpectedFieldIsRefusedAtItsPath();
    void everyRemovedFieldIsRefusedOrReadsAsEmpty();
    void everyLeafOfTheWrongTypeIsRefused();
    void unknownWordsAndUnparseableDatesAreRefused_data();
    void unknownWordsAndUnparseableDatesAreRefused();
    void emptyIdIsRefused_data();
    void emptyIdIsRefused();

private:
    WardPayload m_payload;
    QJsonObject m_json;
};
