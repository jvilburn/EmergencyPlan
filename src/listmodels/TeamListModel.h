#pragma once

#include "Id.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>

class DocumentManager;

class TeamListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ColorRole,
        LeaderIdRole,
        HasLeaderRole,
        MemberCountRole,
        IsEmptyRole
    };
    Q_ENUM(Roles)

    explicit TeamListModel(QObject* parent);

    // Setup
    void setDocumentManager(DocumentManager* documentManager);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // ID list management (called by controller)
    void setTeamIds(const QList<TeamId>& ids);
    const QList<TeamId>& teamIds() const { return m_teamIds; }

    // Lookup
    std::optional<TeamId> teamIdAt(int row) const;
    int rowForId(const TeamId& id) const;

private:
    QList<TeamId> m_teamIds;
    DocumentManager* m_documentManager = nullptr;
};
