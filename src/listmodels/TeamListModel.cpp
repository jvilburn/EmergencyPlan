#include "TeamListModel.h"
#include "DocumentManager.h"

TeamListModel::TeamListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void TeamListModel::setDocumentManager(DocumentManager* documentManager)
{
    m_documentManager = documentManager;
}

int TeamListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_teamIds.size();
}

QVariant TeamListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_teamIds.size())
    {
        return QVariant();
    }

    if (!m_documentManager)
    {
        return QVariant();
    }

    const TeamId& id = m_teamIds.at(index.row());
    std::optional<Team> opt = m_documentManager->document().findTeamById(id);
    if (!opt)
    {
        return QVariant();
    }

    const Team& team = *opt;

    switch (role)
    {
        case Qt::DisplayRole:
        case NameRole:
            return team.name();

        case IdRole:
            return team.id().toString();

        case ColorRole:
            return team.color();

        case LeaderIdRole:
            return team.hasLeader() ? team.leaderId()->toString() : QString();

        case HasLeaderRole:
            return team.hasLeader();

        case MemberCountRole:
            return team.memberCount();

        case IsEmptyRole:
            return team.isEmpty();

        default:
            return QVariant();
    }
}

QHash<int, QByteArray> TeamListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "name";
    roles[ColorRole] = "color";
    roles[LeaderIdRole] = "leaderId";
    roles[HasLeaderRole] = "hasLeader";
    roles[MemberCountRole] = "memberCount";
    roles[IsEmptyRole] = "isEmpty";
    return roles;
}

void TeamListModel::setTeamIds(const QList<TeamId>& ids)
{
    beginResetModel();
    m_teamIds = ids;
    endResetModel();
}

TeamId TeamListModel::teamIdAt(int row) const
{
    if (row >= 0 && row < m_teamIds.size())
    {
        return m_teamIds.at(row);
    }
    return TeamId::fromString(QString());
}

int TeamListModel::rowForId(const TeamId& id) const
{
    return m_teamIds.indexOf(id);
}
