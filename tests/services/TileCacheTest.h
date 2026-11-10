#pragma once

#include <QObject>

/// The tile disk cache keeps a tile's pixels and metadata exactly in QOI, and past a layer's
/// budget deletes that layer's least recently used tiles outside the area around the
/// households, never one inside it.
class TileCacheTest : public QObject
{
    Q_OBJECT

public:
    explicit TileCacheTest(QObject* parent);

private slots:
    void savedTileReadsBackExactlyAndSmaller();
    void newTilePastTheBudgetDeletesTheLeastRecentlyUsed();
    void protectedTilesAreNeitherDeletedNorCounted();
    void coverageHoldsAHouseholdsStreetInDetailAndItsTownCoarsely();
};
