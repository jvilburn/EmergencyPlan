#pragma once

#include "BaseTreeModel.h"
#include "DocumentChange.h"

#include <QList>
#include <QString>

class DocumentManager;
class Filter;

/// Model for ministering tree (multi-level: District → Companionship → Section → Person).
///
/// Structure:
/// - District (top level) - shows "Name (N families/sisters)"
/// - Companionship - shows "Minister1, Minister2 (N)"
/// - SectionHeader - "Ministers" or "Families"/"Sisters" (italic)
/// - Minister/MinisteredFamily/MinisteredSister - individual person/family
/// - ContactDetail - phone, email, address (loaded on demand)
///
/// Contact details are loaded lazily when a person/family node is expanded.
/// This model rebuilds on Full, EqDistrict, EqGroup, RsDistrict, RsGroup, Family scopes.
class MinisteringModel : public BaseTreeModel
{
    Q_OBJECT

public:
    /// Custom roles for accessing item data
    enum Roles
    {
        NodeTypeRole = Qt::UserRole + 1
    };
    Q_ENUM(Roles)

    explicit MinisteringModel(DocumentManager* documentManager,
                               Filter* filter,
                               MinisteringOrg org,
                               QObject* parent = nullptr);
    ~MinisteringModel() override;

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
    /// Used by views to compute map highlights.
    FamilyAssociation relatedFamiliesAt(const QModelIndex& index) const;

    // View-specific accessors
    MinisteringGroupId companionshipIdAt(const QModelIndex& index) const;

    // Lazy loading for contact details
    void loadContactDetails(const QModelIndex& index);
    bool hasContactsLoaded(const QModelIndex& index) const;

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
        std::optional<MinisteringDistrictId> districtId;
        std::optional<MinisteringGroupId> groupId;
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
    void addMinistersSection(TreeNode* companionshipNode, const MinisteringGroupId& groupId);
    void addMinisteredSection(TreeNode* companionshipNode, const MinisteringGroupId& groupId);
    int countMinisteredChildren(TreeNode* companionshipNode) const;
    bool isEQ() const { return m_org == MinisteringOrg::EldersQuorum; }
    QSet<FamilyId> familyIdsForPersons(const QSet<PersonId>& personIds) const;

    QList<TreeNode*> m_districtNodes;  // Top-level nodes (owned)
    DocumentManager* m_documentManager;
    Filter* m_filter;
    MinisteringOrg m_org;
};
