#include "FamilyListModel.h"
#include "DocumentManager.h"
#include "Filter.h"
#include "Family.h"

#include <algorithm>

FamilyListModel::FamilyListModel(DocumentManager* documentManager,
                                 Filter* filter,
                                 QObject* parent)
    : QAbstractListModel(parent)
    , m_documentManager(documentManager)
    , m_filter(filter)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &FamilyListModel::onDocumentChanged);
    connect(m_filter, &Filter::changed,
            this, [this]() { rebuild(); });
    rebuild();
}

void FamilyListModel::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change)
    // TODO: implement surgical updates for Family scope changes
    rebuild();
}

void FamilyListModel::rebuild()
{
    const Document& doc = m_documentManager->document();
    const QHash<QString, Family>& families = doc.families();

    QList<QString> ids;
    ids.reserve(families.size());

    for (auto it = families.begin(); it != families.end(); ++it)
    {
        if (m_filter->passes(doc, it.value()))
        {
            ids.append(it.key());
        }
    }

    std::sort(ids.begin(), ids.end(), [&families](const QString& a, const QString& b) {
        return families.value(a).displayName().toLower()
             < families.value(b).displayName().toLower();
    });

    beginResetModel();
    m_familyIds = ids;
    endResetModel();
}

QStringList FamilyListModel::familyIds() const
{
    return m_familyIds;
}

int FamilyListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_familyIds.size();
}

QVariant FamilyListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_familyIds.size())
    {
        return QVariant();
    }

    if (!m_documentManager)
    {
        return QVariant();
    }

    const QString& id = m_familyIds.at(index.row());
    std::optional<Family> opt = m_documentManager->document().findFamilyById(id);
    if (!opt)
    {
        return QVariant();
    }

    const Family& family = *opt;

    switch (role)
    {
        case Qt::DisplayRole:
        case DisplayNameRole:
            return family.displayName();

        case IdRole:
            return family.id();

        case AddressRole:
            return family.address().multiLine();

        case LatitudeRole:
            return family.latitude().has_value()
                ? QVariant(family.latitude().value())
                : QVariant();

        case LongitudeRole:
            return family.longitude().has_value()
                ? QVariant(family.longitude().value())
                : QVariant();

        case IsMappedRole:
            return family.isMapped();

        case MemberCountRole:
            return family.members().size();

        case DisplayPhoneRole:
            return family.displayPhone();

        case DisplayEmailRole:
            return family.displayEmail();

        case HasContactRole:
            return family.hasContact();

        default:
            return QVariant();
    }
}

QHash<int, QByteArray> FamilyListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[DisplayNameRole] = "displayName";
    roles[AddressRole] = "address";
    roles[LatitudeRole] = "latitude";
    roles[LongitudeRole] = "longitude";
    roles[IsMappedRole] = "isMapped";
    roles[MemberCountRole] = "memberCount";
    roles[DisplayPhoneRole] = "displayPhone";
    roles[DisplayEmailRole] = "displayEmail";
    roles[HasContactRole] = "hasContact";
    return roles;
}

QString FamilyListModel::idAt(int row) const
{
    if (row >= 0 && row < m_familyIds.size())
    {
        return m_familyIds.at(row);
    }
    return QString();
}

int FamilyListModel::rowForId(const QString& id) const
{
    return m_familyIds.indexOf(id);
}
