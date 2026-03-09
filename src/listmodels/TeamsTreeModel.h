#pragma once

#include "BaseTreeModel.h"
#include "DocumentChange.h"

#include <QList>
#include <QString>

class DocumentManager;
class Filter;

/// Model for teams tree (2-level: Team → Member → ContactDetail).
///
/// Level 0: Teams sorted by name, displayed as "Name (N)" where N = member count
/// Level 1: Members in each team, sorted by display name. Leader shown first with "(leader)" suffix.
/// Level 2: Contact details (phone, email, address) - lazy loaded on expand
///
/// Selection key formats:
///   Team         -> {teamId}
///   TeamMember   -> {teamId}:{personId}
///   ContactDetail-> parent's key
class TeamsTreeModel : public BaseTreeModel
{
    Q_OBJECT

public:
    enum Roles
    {
        ItemTypeRole = Qt::UserRole + 1,
        TeamIdRole
    };
    Q_ENUM(Roles)

    explicit TeamsTreeModel(DocumentManager* documentManager,
                            Filter* filter,
                            QObject* parent = nullptr);
    ~TeamsTreeModel() override;

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

    // View-specific accessors (return parent's IDs for ContactDetail nodes)
    std::optional<TeamId> teamIdAt(const QModelIndex& index) const;
    std::optional<PersonId> personIdAt(const QModelIndex& index) const;

    /// Find the first member node belonging to the given family.
    QModelIndex indexForFamilyId(const FamilyId& familyId) const;

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void rebuild();
    void refreshFamilyDisplayText(const FamilyId& familyId);
    void clearNodes();

    struct TreeNode
    {
        ItemType type = ItemType::Invalid;
        TeamId teamId;
        std::optional<PersonId> personId;
        QString displayText;
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;
        bool contactsLoaded = false;

        ~TreeNode() { qDeleteAll(children); }
    };

    TreeNode* nodeFromIndex(const QModelIndex& index) const;

    QList<TreeNode*> m_teamNodes;  // Top-level team nodes (owned)
    DocumentManager* m_documentManager;
    Filter* m_filter;
};
