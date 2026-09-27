#pragma once

#include <QObject>

class CryptoTypesTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructedValuesAreInvalid();
    void aValueIsValidOnlyAtItsOwnSize();
    void equalityComparesTheBytes();

    void fingerprintHexRoundTrips();
    void groupedHexIsFourCharacterGroups();
    void fingerprintFromHexAcceptsTheGroupedForm();
    void fingerprintFromHexRefusesMalformedInput();

    void memberIdHexRoundTrips();
    void memberIdFromHexRefusesMalformedInput();
};
