#pragma once

#include "Id.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>

class DocumentManager;

class TagListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ColorRole,
        LevelRole,
        EntityCountRole,
        IsEmptyRole
    };
    Q_ENUM(Roles)

    explicit TagListModel(QObject* parent);

    // Setup
    void setDocumentManager(DocumentManager* documentManager);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // ID list management (called by controller)
    void setTagIds(const QList<TagId>& ids);
    const QList<TagId>& tagIds() const { return m_tagIds; }

    // Lookup
    std::optional<TagId> tagIdAt(int row) const;
    int rowForId(const TagId& id) const;

private:
    QList<TagId> m_tagIds;
    DocumentManager* m_documentManager = nullptr;
};
