#include "DocumentMetadataTest.h"
#include "DocumentMetadata.h"

#include <QTest>

namespace
{

/// The name of a document holding one ward record for each of names, under made-up unit
/// numbers.
QString wardNameOf(const QStringList& names)
{
    DocumentMetadata metadata;
    for (int i = 0; i < names.size(); ++i)
    {
        Ward ward;
        ward.setUnitNumber(QString::number(100000 + i));
        ward.setName(names[i]);
        metadata.updateWard(ward);
    }
    return metadata.wardName();
}

}  // namespace

DocumentMetadataTest::DocumentMetadataTest(QObject* parent)
    : QObject(parent)
{
}

void DocumentMetadataTest::wardNameListsImportedWardsAlphabetically()
{
    QCOMPARE(wardNameOf({}), QString("Untitled ward"));
    QCOMPARE(wardNameOf({"Tanglewood"}), QString("Tanglewood Ward"));
    QCOMPARE(wardNameOf({"Tanglewood", "Oak"}), QString("Oak and Tanglewood Wards"));
    QCOMPARE(wardNameOf({"Tanglewood", "Oak", "Elm"}), QString("Elm, Oak and Tanglewood Wards"));
}
