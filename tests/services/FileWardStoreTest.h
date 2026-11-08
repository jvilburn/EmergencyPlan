#pragma once

#include <QObject>

class FileWardStoreTest : public QObject
{
    Q_OBJECT

public:
    explicit FileWardStoreTest(QObject* parent);

private slots:
    void eachWardIsOneFileNamedByItsDocumentId();
};
