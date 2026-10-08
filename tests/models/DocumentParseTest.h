#pragma once

#include <QObject>

/// Document::parse, the only reader of a document, over a payload that uses every
/// collection: one damaged element anywhere refuses the whole payload.
class DocumentParseTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aFullPayloadReadsAndWritesBackUnchanged();
    void aDamagedElementInAnyCollectionRefusesTheWhole();
    void aRepeatedIdInAnyCollectionRefusesTheWhole();
    void aCollectionPresentButEmptyIsRefused();
    void anIllFormedImportDateIsRefused();
    void anIdNamingSomethingNotInTheDocumentIsRead();
    void aParsedDocumentAnswersItsLookups();
};
