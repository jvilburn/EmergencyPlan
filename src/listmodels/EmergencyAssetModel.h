#pragma once

#include "BaseTreeModel.h"
#include "DocumentChange.h"
#include "ResponseArea.h"

#include <QList>
#include <QString>

class DocumentManager;
class Filter;

/// Model for emergency assets tree (3-level: Asset → Person → ContactDetail).
///
/// Level 0: Assets sorted by name, displayed as "Name (N)" where N = people count
/// Level 1: People in each asset, sorted by display name
/// Level 2: Contact details (phone, email, address) - lazy loaded on expand
///
/// This model rebuilds on Full, EmergencyAsset, and Family add/remove changes.
/// Family updates only refresh display text (no structural rebuild).
class EmergencyAssetModel : public BaseTreeModel
{
    Q_OBJECT

public:
    /// Custom roles for accessing item data
    enum Roles
    {
        ItemTypeRole = Qt::UserRole + 1,
        AssetIdRole  // For Person items: the parent asset's ID
    };
    Q_ENUM(Roles)

    explicit EmergencyAssetModel(DocumentManager* documentManager,
                                     Filter* filter,
                                     ResponseArea area,
                                     QObject* parent = nullptr);
    ~EmergencyAssetModel() override;

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
    std::optional<EmergencyAssetId> assetIdAt(const QModelIndex& index) const;
    std::optional<PersonId> personIdAt(const QModelIndex& index) const;

    /// Find the first person node belonging to the given family.
    QModelIndex indexForFamilyId(const FamilyId& familyId) const;
    ResponseArea area() const { return m_area; }

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
        EmergencyAssetId assetId;             // Asset ID (for Asset nodes: self, for Person/Contact: parent asset)
        std::optional<PersonId> personId;     // Person ID (for Person and ContactDetail nodes)
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

    QList<TreeNode*> m_assetNodes;  // Top-level nodes (owned)
    DocumentManager* m_documentManager;
    Filter* m_filter;
    ResponseArea m_area;
};
