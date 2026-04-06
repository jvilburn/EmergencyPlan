#include "TagListModel.h"
#include "DocumentManager.h"
#include "Tag.h"

TagListModel::TagListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int TagListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_tagIds.size();
}

QVariant TagListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tagIds.size())
    {
        return QVariant();
    }

    const TagId& id = m_tagIds.at(index.row());
    std::optional<Tag> opt = DocumentManager::instance()->document().findTagById(id);
    if (!opt)
    {
        return QVariant();
    }

    const Tag& tag = *opt;

    switch (role)
    {
        case Qt::DisplayRole:
        case NameRole:
            return tag.name();

        case IdRole:
            return tag.id().toString();

        case ColorRole:
            return tag.color();

        case LevelRole:
            return tagLevelToString(tag.level());

        case EntityCountRole:
            return tag.isPersonLevel() ? tag.personIds().size() : tag.familyIds().size();

        case IsEmptyRole:
            return tag.isEmpty();

        default:
            return QVariant();
    }
}

QHash<int, QByteArray> TagListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "name";
    roles[ColorRole] = "color";
    roles[LevelRole] = "level";
    roles[EntityCountRole] = "entityCount";
    roles[IsEmptyRole] = "isEmpty";
    return roles;
}

void TagListModel::setTagIds(const QList<TagId>& ids)
{
    beginResetModel();
    m_tagIds = ids;
    endResetModel();
}

std::optional<TagId> TagListModel::tagIdAt(int row) const
{
    if (row >= 0 && row < m_tagIds.size())
    {
        return m_tagIds.at(row);
    }
    return std::nullopt;
}

int TagListModel::rowForId(const TagId& id) const
{
    return m_tagIds.indexOf(id);
}
