#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QString>

#include "DocumentChange.h"

class DocumentManager;

/// Model for special needs tree (2-level: Person → ContactDetail).
///
/// Level 0: People with special needs, displayed as "PersonName - note" or just "PersonName"
/// Level 1: Contact details (phone, email, address) - lazy loaded on expand
///
/// Sorted alphabetically by person display name.
class NeedsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    /// Node types in the tree
    enum class ItemType
    {
        Invalid,
        Person,
        ContactDetail
    };
    Q_ENUM(ItemType)

    // Custom roles
    enum Roles
    {
        PersonIdRole = Qt::UserRole + 1,
        FamilyIdRole
    };
    Q_ENUM(Roles)

    explicit NeedsModel(DocumentManager* documentManager, QObject* parent = nullptr);
    ~NeedsModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;

    // Lazy loading for contact details
    void loadContactDetails(const QModelIndex& index);

    // View-specific accessors
    QString personIdAt(const QModelIndex& index) const;
    QString familyIdAt(const QModelIndex& index) const;
    QModelIndex indexForPersonId(const QString& personId) const;
    ItemType itemTypeAt(const QModelIndex& index) const;

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void rebuild();
    bool shouldRebuild(const DocumentChange& change) const;

    struct TreeNode
    {
        ItemType type = ItemType::Invalid;
        QString personId;
        QString familyId;
        QString displayText;
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;
        bool contactsLoaded = false;

        ~TreeNode()
        {
            qDeleteAll(children);
        }
    };

    TreeNode* nodeFromIndex(const QModelIndex& index) const;
    void clearNodes();

    QList<TreeNode*> m_personNodes;  // Top-level person nodes (owned)
    DocumentManager* m_documentManager;
};
