#pragma once

#include <QObject>

/// Where a .wardplanning file opened by path goes, by its header.
class FileOpenRouterTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void eachFileTakesItsRoute_data();
    void eachFileTakesItsRoute();
};
