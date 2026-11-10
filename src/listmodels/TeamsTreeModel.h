#pragma once

#include "BaseTreeModel.h"
#include "Id.h"

#include <QList>
#include <QString>
#include <optional>

class Filter;
class ResponseTask;

/// Model for teams tree (2-level: Team -> Member -> ContactDetail).
/// During emergencies, adds the tasks assigned to each team under it, and an "Unassigned
/// Tasks" section for the tasks with no assignment. A task assigned to a person is shown in
/// the Tasks tab only.
///
/// Level 0: Teams sorted by name, displayed as "Name (N)" where N = member count
/// Level 1: Members in each team, sorted by display name. Leader shown first with "(leader)" suffix.
///          During emergencies: task rows appended after members.
/// Level 2: Contact details (phone, email, address) - lazy loaded on expand
///
/// It rebuilds on every document change.
///
/// Selection key formats:
///   Team         -> {teamId}
///   TeamMember   -> {teamId}:{personId}
///   TaskRow      -> task:{taskId}
///   ContactDetail-> parent's key
class TeamsTreeModel : public BaseTreeModel
{
    Q_OBJECT

public:
    enum Roles
    {
        ItemTypeRole = Qt::UserRole + 1,
        TeamIdRole,
        TaskIdRole,
        FamilyIdRole
    };
    Q_ENUM(Roles)

    explicit TeamsTreeModel(Filter* filter,
                            QObject* parent);
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

private:
    void rebuild();
    void clearNodes();

    struct TreeNode
    {
        ItemType type = ItemType::Invalid;
        std::optional<TeamId> teamId;
        std::optional<PersonId> personId;
        std::optional<TaskId> taskId;
        std::optional<FamilyId> familyId;
        QString displayText;
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;
        bool contactsLoaded = false;
        bool taskResolved = false;

        ~TreeNode() { qDeleteAll(children); }
    };

    TreeNode* nodeFromIndex(const QModelIndex& index) const;

    /// A row under parent showing task.
    static TreeNode* newTaskNode(const ResponseTask& task, TreeNode* parent);

    /// The Unassigned Tasks section holding a row for each of tasks.
    static TreeNode* newUnassignedHeader(const QList<ResponseTask>& tasks);

    QList<TreeNode*> m_teamNodes;  // Top-level team nodes (owned)
    Filter* m_filter;
};
