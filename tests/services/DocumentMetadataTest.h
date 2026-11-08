#pragma once

#include <QObject>

class DocumentMetadataTest : public QObject
{
    Q_OBJECT

public:
    explicit DocumentMetadataTest(QObject* parent);

private slots:
    void wardNameListsImportedWardsAlphabetically();
};
