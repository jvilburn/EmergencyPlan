#pragma once

#include <QObject>

class AppSettingsTest : public QObject
{
    Q_OBJECT

public:
    explicit AppSettingsTest(QObject* parent);

private slots:
    void missingFileReadsAsEveryDefault();
    void eachValueSurvivesANewSettingsOnTheSameFile();
};
