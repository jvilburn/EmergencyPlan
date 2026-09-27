#pragma once

#include <QObject>

/// Document::parse, the only reader of a document, over a payload that uses every
/// collection: one damaged element anywhere refuses the whole payload, and so does
/// any reference to something the document does not contain.
class DocumentParseTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aFullPayloadReadsAndWritesBackUnchanged();
    void aDamagedElementInAnyCollectionRefusesTheWhole();
    void aRepeatedIdInAnyCollectionRefusesTheWhole();
    void aPersonInTwoFamiliesIsRefused();
    void aTagNamingAnUnknownPersonOrFamilyIsRefused();
    void aTeamNamingAnUnknownPersonIsRefused();
    void aDistrictNamingAGroupNotInItsOrganisationIsRefused();
    void anIllFormedImportDateIsRefused();
    void jsonServiceReadsWhatItWrites();
};
