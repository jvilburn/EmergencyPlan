#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

class DocumentManager;
class Filter;

/// List model exposing filtered, sorted families.
/// Each view creates its own instance with its own Filter.
class FamilyListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        DisplayNameRole,
        AddressRole,
        LatitudeRole,
        LongitudeRole,
        IsMappedRole,
        MemberCountRole,
        DisplayPhoneRole,
        DisplayEmailRole,
        HasContactRole
    };
    Q_ENUM(Roles)

    explicit FamilyListModel(DocumentManager* documentManager,
                             Filter* filter,
                             QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Lookup
    QString idAt(int row) const;
    int rowForId(const QString& id) const;

    // Access to visible family IDs (for map integration)
    QStringList familyIds() const;

private slots:
    void rebuild();

private:
    QList<QString> m_familyIds;
    DocumentManager* m_documentManager;
    Filter* m_filter;
};
