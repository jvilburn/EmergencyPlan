#pragma once

#include "BaseTreeModel.h"
#include "DocumentChange.h"

#include <QList>
#include <QSet>
#include <QString>

class DocumentManager;
class EmergencyManager;
class Filter;

/// Tree model exposing filtered, sorted families with full hierarchical structure.
/// Structure:
///   - Family (level 0) - expandable
///     - Member (level 1) - expandable, shows name
///       - MemberDetail (level 2) - phone, email, callings, age
///     - Address (level 1) - text only
///     - Phone (level 1) - text only
///     - Actions (level 1) - widget row for Edit/Delete buttons
class FamilyTreeModel : public BaseTreeModel
{
    Q_OBJECT

public:
    /// Row types in the tree
    enum class RowType
    {
        Family,       // Top-level family row
        Member,       // Family member (expandable)
        MemberDetail, // Member's phone/email/callings/age
        Address,      // Family address
        Phone,        // Family phone
        Actions       // Edit/Delete buttons
    };
    Q_ENUM(RowType)

    enum Roles
    {
        RowTypeRole = Qt::UserRole + 1,
        FamilyIdRole,
        MemberIndexRole,
        DetailTypeRole,
        ResponseStatusRole
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

    explicit FamilyTreeModel(DocumentManager* documentManager,
                             EmergencyManager* emergencyManager,
                             Filter* filter,
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

    DocumentManager* documentManager() const { return m_documentManager; }

    /// Enable checkbox mode for multi-select dialogs.
    void setCheckable(bool checkable);

    /// Set/get checked family IDs (only meaningful when checkable).
    void setCheckedFamilyIds(const QSet<FamilyId>& ids);
    QSet<FamilyId> checkedFamilyIds() const;

    // QAbstractItemModel overrides for checkboxes
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

signals:
    void familyListChanged();

private slots:
    void onDocumentChanged(const DocumentChange& change);
    void onFamilyStatusChanged(const FamilyId& familyId);
    void rebuild();

private:
    // Surgical update methods
    void updateFamilyRow(const FamilyId& familyId);
    void insertFamilyRow(const FamilyId& familyId);
    void removeFamilyRow(const FamilyId& familyId);

    /// Internal tree node structure
    struct TreeNode
    {
        RowType type;
        int familyIndex = -1;   // Index into m_familyIds
        int memberIndex = -1;   // For Member/MemberDetail rows
        DetailType detailType = DetailType::Phone; // For MemberDetail rows
        QString displayText;    // Cached display text
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;

        ~TreeNode()
        {
            qDeleteAll(children);
        }
    };

    void clearNodes();
    void buildFamilyNode(int familyIndex);
    TreeNode* nodeFromIndex(const QModelIndex& index) const;

    QList<FamilyId> m_familyIds;
    QList<TreeNode*> m_familyNodes;  // Top-level nodes (owned)
    DocumentManager* m_documentManager;
    EmergencyManager* m_emergencyManager;
    Filter* m_filter;
    bool m_checkable = false;
    QSet<FamilyId> m_checkedIds;
};
