#pragma once

#include "BaseTreeModel.h"
#include "DocumentManager.h"

#include <QList>
#include <QSet>
#include <QString>

class Filter;

/// Tree model exposing filtered, sorted families with full hierarchical structure.
/// Structure:
///   - Family (level 0) - expandable
///     - Member (level 1) - expandable, shows name
///       - MemberDetail (level 2) - phone, email, callings, age
///     - Address (level 1) - text only
///     - Phone (level 1) - text only
///     - ContactAttempt and Task (level 1) - while an emergency is in progress, the family's
///       attempts and tasks, each list under its header row
///     - Actions (level 1) - widget row for Edit/Delete buttons
///
/// A change to one family replaces that family's rows; a change to its attempts or tasks,
/// and an emergency starting or ending, replace only the attempt and task rows, so expanded
/// members and the scroll position stay. Each of these shows or hides the family by the
/// filter.
class FamilyTreeModel : public BaseTreeModel
{
    Q_OBJECT

public:
    /// Row types in the tree
    enum class RowType
    {
        Family,           // Top-level family row
        Member,           // Family member (expandable)
        MemberDetail,     // Member's phone/email/callings/age
        Address,          // Family address
        Phone,            // Family phone
        ContactAttempt,   // Emergency: contact attempt record
        Task,             // Emergency: task record
        Actions           // Edit/Delete buttons
    };
    Q_ENUM(RowType)

    enum Roles
    {
        RowTypeRole = Qt::UserRole + 1,
        FamilyIdRole,
        MemberIndexRole,
        DetailTypeRole,
        ResponseStatusRole,
        TaskIdRole
    };
    Q_ENUM(Roles)

    /// Detail types for MemberDetail rows
    enum class DetailType
    {
        Phone,
        AltPhone,
        Email,
        Callings,
        Age
    };
    Q_ENUM(DetailType)

    explicit FamilyTreeModel(Filter* filter,
                             bool checkable,
                             QObject* parent);
    ~FamilyTreeModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    SelectionKey selectionKeyAt(const QModelIndex& index) const override;

    // Lookup helpers
    std::optional<FamilyId> familyIdAt(const QModelIndex& index) const;
    std::optional<PersonId> personIdAt(const QModelIndex& index) const;
    QModelIndex indexForFamilyId(const FamilyId& id) const;
    RowType rowTypeAt(const QModelIndex& index) const;

    // Access to visible family IDs (for map integration)
    QList<FamilyId> familyIds() const;

    /// Set/get checked family IDs (only meaningful when checkable).
    void setCheckedFamilyIds(const QSet<FamilyId>& ids);
    QSet<FamilyId> checkedFamilyIds() const;

    // QAbstractItemModel overrides for checkboxes
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

private slots:
    void rebuild();

private:

    /// Internal tree node structure
    struct TreeNode
    {
        RowType type;
        int familyIndex = -1;   // Index into m_familyIds
        int memberIndex = -1;   // For Member/MemberDetail rows
        DetailType detailType = DetailType::Phone; // For MemberDetail rows
        std::optional<TaskId> taskId;  // For Task rows
        bool taskResolved = false;     // For Task rows: cached resolved state
        QString displayText;    // Cached display text
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;

        ~TreeNode()
        {
            qDeleteAll(children);
        }
    };

    void clearNodes();

    static TreeNode* newNode(RowType type, TreeNode* parent, int familyIndex, const QString& text);
    static void appendDetail(TreeNode* memberNode, DetailType detailType, const QString& text);

    /// The family's node at familyIndex, with its child rows.
    TreeNode* buildFamilyNode(int familyIndex, const Family& family) const;

    /// The family's child rows: members, address, phone, attempts and tasks, actions.
    QList<TreeNode*> childNodes(TreeNode* familyNode, const Family& family) const;

    /// The family's attempt and task rows while an emergency is in progress, otherwise none.
    QList<TreeNode*> emergencyNodes(TreeNode* familyNode, const FamilyId& familyId) const;

    TreeNode* nodeFromIndex(const QModelIndex& index) const;

    QList<FamilyId> m_familyIds;
    QList<TreeNode*> m_familyNodes;  // Top-level nodes (owned)
    Filter* m_filter;
    bool m_checkable = false;
    QSet<FamilyId> m_checkedIds;
};
