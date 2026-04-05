#pragma once

#include "BaseTreeModel.h"
#include "DocumentChange.h"

#include <QList>
#include <QString>

class Filter;

/// Model for special needs tree (2-level: Person → ContactDetail).
///
/// Level 0: People with special needs, displayed as "PersonName - note" or just "PersonName"
/// Level 1: Contact details (phone, email, address) - lazy loaded on expand
///
/// Sorted alphabetically by person display name.
class NeedsModel : public BaseTreeModel
{
    Q_OBJECT

public:
    // Custom roles
    enum Roles
    {
        PersonIdRole = Qt::UserRole + 1,
        FamilyIdRole
    };
    Q_ENUM(Roles)

    explicit NeedsModel(Filter* filter,
                        QObject* parent);
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

    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    SelectionKey selectionKeyAt(const QModelIndex& index) const override;

    /// Returns family associations for the given index.
    FamilyAssociation relatedFamiliesAt(const QModelIndex& index) const;

    // View-specific accessors
    std::optional<PersonId> personIdAt(const QModelIndex& index) const;
    std::optional<FamilyId> familyIdAt(const QModelIndex& index) const;
    QModelIndex indexForPersonId(const PersonId& personId) const;

    /// Find the first person node belonging to the given family.
    QModelIndex indexForFamilyId(const FamilyId& familyId) const;

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void rebuild();
    bool shouldRebuild(const DocumentChange& change) const;

    struct TreeNode
    {
        ItemType type = ItemType::Invalid;
        PersonId personId;
        FamilyId familyId;
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
    Filter* m_filter;
};
