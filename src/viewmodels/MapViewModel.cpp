#include "MapViewModel.h"
#include "DocumentManager.h"
#include "Family.h"
#include "Document.h"
#include "Id.h"


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
    const QHash<FamilyId, Family>& familyMap = doc.families();

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
    map["id"] = family.id().toString();
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

MarkerRenderer::MarkerIcons MapViewModel::familyIcons(const FamilyId& familyId) const
{
    return m_familyIcons.value(familyId);
}
