#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QString>

#include "DocumentChange.h"
#include "MinisteringModel.h"  // For NodeType enum

class DocumentManager;

/// Model for unassigned ministering tree (2-level: Header → Person/Family).
///
/// Structure:
/// - UnassignedHeader (top level) - shows "Unassigned (N families/sisters)"
/// - MinisteredFamily (EQ) or MinisteredSister (RS) - individual items
/// - ContactDetail - phone, email, address (loaded on demand)
///
/// This model rebuilds on Full, EqGroup, RsGroup, Family scopes.
class UnassignedMinisteringModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    // Reuse NodeType from MinisteringModel
    using NodeType = MinisteringModel::NodeType;

    /// Custom roles for accessing item data
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NodeTypeRole,
        SecondaryIdRole
    };
    Q_ENUM(Roles)

    explicit UnassignedMinisteringModel(DocumentManager* documentManager,
                                         bool isEQ,
                                         QObject* parent = nullptr);
    ~UnassignedMinisteringModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // View-specific accessors
    QString idAt(const QModelIndex& index) const;
    /// Returns the node type at the given index, or NodeType::Invalid for invalid indexes.
    NodeType nodeTypeAt(const QModelIndex& index) const;

    // Lazy loading for contact details
    void loadContactDetails(const QModelIndex& index);
    bool hasContactsLoaded(const QModelIndex& index) const;

    // Check if there are any unassigned items
    bool hasUnassigned() const;

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void rebuild();
    bool shouldRebuild(const DocumentChange& change) const;
    void clearNodes();

    /// Internal tree node structure
    struct TreeNode
    {
        NodeType type;
        QString id;
        QString displayText;
        QString secondaryId;
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;
        bool contactsLoaded = false;

        ~TreeNode()
        {
            qDeleteAll(children);
        }
    };

    TreeNode* nodeFromIndex(const QModelIndex& index) const;

    TreeNode* m_headerNode = nullptr;  // Single top-level node (owned)
    DocumentManager* m_documentManager;
    bool m_isEQ;
};
