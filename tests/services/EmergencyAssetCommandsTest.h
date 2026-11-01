#pragma once

#include <QObject>

/// Emergency asset commands hold their changed name and people changes only, so two users'
/// edits to one asset's people both land.
class EmergencyAssetCommandsTest : public QObject
{
    Q_OBJECT

public:
    explicit EmergencyAssetCommandsTest(QObject* parent);

private slots:
    void assetEditsAddingDifferentPeopleBothLand();
    void removalOfAPersonAlreadyRemovedIsDone();
};
