#pragma once

#include <QObject>

/// This device's identity, as a file of its secret halves.
class IdentityFileTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // The record
    void aRecordRoundTripsWithAndWithoutAPerson();
    void onlyTheSecretHalvesAreWritten();
    void aRecordWithASecretOfTheWrongSizeIsRefused();
    void aRecordWithAnUnknownFieldIsRefused();
    void onlyThisBuildsVersionReads();

    // The file
    void aMissingFileIsNotFound();
    void aStoredIdentityLoadsBack();
    void aDamagedFileFailsAndIsLeftAlone();
    void theFileIsReadableOnlyByItsOwner();
    void aPathThatCannotBeOpenedFails();
    void aStoreThatCannotWriteFails();
    void aRecordThatIsNotWholeIsNotStored();
    void aNewerVersionIsToldFromADamagedFile();
    void movingAsideKeepsTheFileBesideItself();
};
