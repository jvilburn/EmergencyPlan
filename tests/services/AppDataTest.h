#pragma once

#include <QObject>

class AppDataTest : public QObject
{
    Q_OBJECT

public:
    explicit AppDataTest(QObject* parent);

private slots:
    void wardsAndKeysAreSiblingFoldersOfTheRoot();
};
