#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QString>

class DocumentManager;
class Filter;

/// Tree model exposing filtered, sorted families with full hierarchical structure.
/// Structure:
///   - Family (level 0) - expandable
///     - Member (level 1) - expandable, shows name
///       - MemberDetail (level 2) - phone, email, callings, age
///     - Address (level 1) - text only
///     - Phone (level 1) - text only
///     - Actions (level 1) - widget row for Edit/Delete buttons
class FamilyTreeModel : public QAbstractItemModel
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
        IdRole = Qt::UserRole + 1,
        RowTypeRole,
        FamilyIdRole,
        MemberIndexRole,
        DetailTypeRole
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
                             Filter* filter,
                             QObject* parent = nullptr);
    ~FamilyTreeModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Lookup helpers
    QString familyIdAt(const QModelIndex& index) const;
    QModelIndex indexForFamilyId(const QString& id) const;
    RowType rowTypeAt(const QModelIndex& index) const;

    // Access to visible family IDs (for map integration)
    QStringList familyIds() const;

    DocumentManager* documentManager() const { return m_documentManager; }

signals:
    void familyListChanged();

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

    QList<QString> m_familyIds;
    QList<TreeNode*> m_familyNodes;  // Top-level nodes (owned)
    DocumentManager* m_documentManager;
    Filter* m_filter;
};
