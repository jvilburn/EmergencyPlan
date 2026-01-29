#pragma once

#include "DocumentChange.h"

#include <QAbstractItemModel>
#include <QList>
#include <QString>

class DocumentManager;

/// Model for special needs list (flat list of people with special needs).
///
/// Display format: "PersonName - note" (or just "PersonName" if note is empty).
/// Sorted alphabetically by person display name.
///
/// This is a flat list model - all items are top-level, no hierarchy.
class NeedsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit NeedsModel(DocumentManager* documentManager, QObject* parent = nullptr);

    // Custom roles
    enum Roles
    {
        PersonIdRole = Qt::UserRole + 1,
        FamilyIdRole
    };
    Q_ENUM(Roles)

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // View-specific accessors
    QString personIdAt(const QModelIndex& index) const;
    QString familyIdAt(const QModelIndex& index) const;
    QModelIndex indexForPersonId(const QString& personId) const;

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void rebuild();
    bool shouldRebuild(const DocumentChange& change) const;

    struct NeedEntry
    {
        QString personId;
        QString familyId;
        QString displayText;  // "Name - note" or just "Name"
        QString sortKey;      // lowercase display name for sorting
    };

    QList<NeedEntry> m_entries;
    DocumentManager* m_documentManager;
};
