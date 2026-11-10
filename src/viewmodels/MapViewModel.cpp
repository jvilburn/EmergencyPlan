#include "MapViewModel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "Family.h"

MapViewModel::MapViewModel(QObject* parent)
    : QObject(parent)
{
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &MapViewModel::updateFamilies);
    updateFamilies();
}

void MapViewModel::updateFamilies()
{
    m_families.clear();
    m_familyIcons.clear();

    const Document& doc = DocumentManager::instance()->document();
    for (const Family& family : doc.families())
    {
        // Computed here rather than in the paint loop; the unmapped panel shows them too
        m_familyIcons.insert(family.id(), MarkerRenderer::computeFamilyIcons(family.id(), doc));
        std::optional<Coordinates> coordinates = doc.coordinatesFor(family.address());
        if (coordinates.has_value())
        {
            m_families.append(MappedFamily{family.id(), *coordinates});
        }
    }

    emit familiesChanged();
}

MarkerRenderer::MarkerIcons MapViewModel::familyIcons(const FamilyId& familyId) const
{
    return m_familyIcons.value(familyId);
}
