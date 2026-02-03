#include "MapViewModel.h"
#include "DocumentManager.h"
#include "Family.h"
#include "Document.h"

#include <algorithm>
#include <cmath>

MapViewModel::MapViewModel(DocumentManager* docManager, QObject* parent)
    : QObject(parent)
    , m_docManager(docManager)
{
    connect(m_docManager, &DocumentManager::documentChanged,
            this, &MapViewModel::onDocumentChanged);

    // Initial load
    updateFamilies();
}

void MapViewModel::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change)
    // TODO: optimize for Family scope changes
    updateFamilies();
}

void MapViewModel::updateFamilies()
{
    m_families.clear();
    m_familyIcons.clear();

    const Document& doc = m_docManager->document();
    const QHash<QString, Family>& familyMap = doc.families();

    for (auto it = familyMap.begin(); it != familyMap.end(); ++it)
    {
        const Family& family = it.value();

        // Only include mapped families (those with valid coordinates)
        if (family.isMapped())
        {
            m_families.append(familyToVariant(family));
            // Pre-compute marker icons (expensive operation moved out of paint loop)
            m_familyIcons.insert(family.id(),
                                 MarkerRenderer::computeFamilyIcons(family.id(), doc));
        }
    }

    emit familiesChanged();
}

QVariantMap MapViewModel::familyToVariant(const Family& family) const
{
    QVariantMap map;
    map["id"] = family.id();
    map["name"] = family.displayName();
    map["latitude"] = family.latitude().value_or(0.0);
    map["longitude"] = family.longitude().value_or(0.0);
    map["address"] = family.address().multiLine();
    map["phone"] = QString(family.displayPhone());
    map["email"] = family.displayEmail();
    map["memberCount"] = family.members().size();
    map["isMapped"] = family.isMapped();

    // Callings as comma-separated string
    QStringList callings = family.allCallings();
    map["callings"] = callings.join(", ");

    return map;
}

QVariantList MapViewModel::families() const
{
    return m_families;
}

void MapViewModel::setUseSatelliteView(bool satellite)
{
    if (m_useSatelliteView != satellite)
    {
        m_useSatelliteView = satellite;
        emit satelliteViewChanged();
    }
}

void MapViewModel::setCenterLat(double lat)
{
    if (!qFuzzyCompare(m_centerLat, lat))
    {
        m_centerLat = lat;
        emit centerChanged();
    }
}

void MapViewModel::setCenterLng(double lng)
{
    if (!qFuzzyCompare(m_centerLng, lng))
    {
        m_centerLng = lng;
        emit centerChanged();
    }
}

void MapViewModel::setZoom(double zoom)
{
    if (!qFuzzyCompare(m_zoom, zoom))
    {
        m_zoom = zoom;
        emit zoomChanged();
    }
}

void MapViewModel::setSelectedFamilyId(const QString& id)
{
    if (m_selectedId != id)
    {
        m_selectedId = id;
        emit selectionChanged();
    }
}

void MapViewModel::selectFamily(const QString& id)
{
    setSelectedFamilyId(id);
    emit familyClicked(id);
}

void MapViewModel::centerOnFamily(const QString& id)
{
    const Document& doc = m_docManager->document();
    const QHash<QString, Family>& familyMap = doc.families();

    auto it = familyMap.find(id);
    if (it != familyMap.end())
    {
        const Family& family = it.value();
        if (family.isMapped())
        {
            emit centerOnLocation(family.latitude().value(),
                                  family.longitude().value(),
                                  15.0);
        }
    }
}

void MapViewModel::fitAllFamilies()
{
    if (m_families.isEmpty())
    {
        return;
    }

    double minLat = 90.0;
    double maxLat = -90.0;
    double minLng = 180.0;
    double maxLng = -180.0;

    for (const QVariant& var : m_families)
    {
        QVariantMap map = var.toMap();
        double lat = map["latitude"].toDouble();
        double lng = map["longitude"].toDouble();

        minLat = std::min(minLat, lat);
        maxLat = std::max(maxLat, lat);
        minLng = std::min(minLng, lng);
        maxLng = std::max(maxLng, lng);
    }

    // Add 10% padding
    double latRange = maxLat - minLat;
    double lngRange = maxLng - minLng;
    double latPadding = latRange * 0.1;
    double lngPadding = lngRange * 0.1;

    emit fitBounds(minLat - latPadding, minLng - lngPadding,
                   maxLat + latPadding, maxLng + lngPadding);
}

void MapViewModel::setHighlighting(const QVariantMap& ids)
{
    if (m_highlightedIds != ids)
    {
        m_highlightedIds = ids;
        emit highlightingChanged();
    }
}

void MapViewModel::clearHighlighting()
{
    if (!m_highlightedIds.isEmpty())
    {
        m_highlightedIds.clear();
        emit highlightingChanged();
    }
}

void MapViewModel::mapClicked(double /*lat*/, double /*lng*/)
{
    // Deselect when clicking on empty map area
    if (!m_selectedId.isEmpty())
    {
        setSelectedFamilyId(QString());
    }
}

MarkerRenderer::MarkerIcons MapViewModel::familyIcons(const QString& familyId) const
{
    return m_familyIcons.value(familyId);
}
