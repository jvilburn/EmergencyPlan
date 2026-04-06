#pragma once

#include "BaseTreeModel.h"
#include "DocumentChange.h"

#include <QList>
#include <QString>

class Filter;

/// Model for unassigned ministering tree (2-level: Header → Person/Family).
///
/// Structure:
/// - UnassignedHeader (top level) - shows "Unassigned (N families/sisters)"
/// - MinisteredFamily (EQ) or MinisteredSister (RS) - individual items
/// - ContactDetail - phone, email, address (loaded on demand)
///
/// This model rebuilds on Full, EqGroup, RsGroup, and Family add/remove changes.
/// Family updates only refresh display text (no structural rebuild).
class UnassignedMinisteringModel : public BaseTreeModel
{
    Q_OBJECT

public:
    /// Custom roles for accessing item data
    enum Roles
    {
        NodeTypeRole = Qt::UserRole + 1
    };
    Q_ENUM(Roles)

    explicit UnassignedMinisteringModel(Filter* filter,
                                         MinisteringOrg org,
                                         QObject* parent);
    ~UnassignedMinisteringModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;

    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    SelectionKey selectionKeyAt(const QModelIndex& index) const override;

    /// Returns family associations for the given index.
    FamilyAssociation relatedFamiliesAt(const QModelIndex& index) const;

    /// Find the first MinisteredFamily or MinisteredSister node matching familyId.
    QModelIndex indexForFamilyId(const FamilyId& familyId) const;

    // Lazy loading for contact details
    void loadContactDetails(const QModelIndex& index);
    bool hasContactsLoaded(const QModelIndex& index) const;

    // Check if there are any unassigned items
    bool hasUnassigned() const;

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void rebuild();
    void refreshFamilyDisplayText(const FamilyId& familyId);
    void clearNodes();

    /// Internal tree node structure
    struct TreeNode
    {
        ItemType type;
        std::optional<PersonId> personId;
        std::optional<FamilyId> familyId;
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
    bool isEQ() const { return m_org == MinisteringOrg::EldersQuorum; }
    QSet<FamilyId> familyIdsForPersons(const QSet<PersonId>& personIds) const;
    QSet<FamilyId> unassignedFamilyIds() const;
    QSet<PersonId> unassignedSisterIds() const;

    TreeNode* m_headerNode = nullptr;  // Single top-level node (owned)
    Filter* m_filter;
    MinisteringOrg m_org;
};
