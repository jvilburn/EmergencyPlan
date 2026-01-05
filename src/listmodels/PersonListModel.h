#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

class DocumentManager;
class Filter;

/// List model exposing filtered, sorted persons.
/// Each view creates its own instance with its own Filter.
class PersonListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        DisplayNameRole,
        GivenNamesRole,
        SurnameRole,
        PhoneRole,
        AltPhoneRole,
        EmailRole,
        GenderRole,
        AgeRole,
        IsChildRole,
        BirthDateDisplayRole,
        AgeDisplayRole,
        CallingsRole,
        HasCallingsRole,
        HasContactRole,
        FamilyIdRole
    };
    Q_ENUM(Roles)

    explicit PersonListModel(DocumentManager* documentManager,
                             Filter* filter,
                             QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Lookup
    QString idAt(int row) const;
    int rowForId(const QString& id) const;

    // Access to visible person IDs
    QStringList personIds() const;

private slots:
    void rebuild();

private:
    QList<QString> m_personIds;
    DocumentManager* m_documentManager;
    Filter* m_filter;
};
