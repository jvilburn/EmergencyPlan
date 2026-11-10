#pragma once

#include <QHash>
#include <QList>
#include <QObject>

#include "Coordinates.h"
#include "Id.h"
#include "MarkerRenderer.h"

/// The families the map shows, and every family's marker icons, kept up to date with the
/// document.
class MapViewModel : public QObject
{
    Q_OBJECT

public:
    /// A family the map shows: one whose address has coordinates.
    struct MappedFamily
    {
        FamilyId id;
        Coordinates position;
    };

    explicit MapViewModel(QObject* parent);

    const QList<MappedFamily>& families() const { return m_families; }

    /// The family's marker icons, computed when the document changes; mapped or not.
    MarkerRenderer::MarkerIcons familyIcons(const FamilyId& familyId) const;

signals:
    void familiesChanged();

private:
    void updateFamilies();

    QList<MappedFamily> m_families;
    QHash<FamilyId, MarkerRenderer::MarkerIcons> m_familyIcons;
};
