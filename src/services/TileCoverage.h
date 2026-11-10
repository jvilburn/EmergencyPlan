#pragma once

#include "Coordinates.h"
#include "TileTypes.h"

#include <QList>
#include <QSet>

/// The tiles around the ward's households: at each zoom, every tile within a radius of a
/// household, a radius that shrinks as zoom grows, so a household's street is held in
/// detail and the land around it more coarsely. The cache never trims these tiles.
namespace TileCoverage
{
    /// The radius held around a household at zoom.
    double radiusMeters(int zoom);

    /// The tiles, for zooms 0 to maxZoom, within the radius of any of positions, as street
    /// layer ids: a tile of either layer is covered when the street tile at its place is.
    QSet<TileId> around(const QList<Coordinates>& positions, int maxZoom);

    /// The street layer id at id's place, as around() holds it.
    inline TileId placeOf(TileId id) { return TileId(TileLayer::Street, id.zoom(), id.x(), id.y()); }
}
