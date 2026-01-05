#pragma once

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

    explicit TagListModel(QObject* parent = nullptr);

    // Setup
    void setDocumentManager(DocumentManager* documentManager);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // ID list management (called by controller)
    void setTagIds(const QList<QString>& ids);
    const QList<QString>& tagIds() const { return m_tagIds; }

    // Lookup
    QString idAt(int row) const;
    int rowForId(const QString& id) const;

private:
    QList<QString> m_tagIds;
    DocumentManager* m_documentManager = nullptr;
};
