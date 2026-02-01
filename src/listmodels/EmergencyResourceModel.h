#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QString>

#include "DocumentChange.h"
#include "ResponseArea.h"

class DocumentManager;

/// Model for emergency resources tree (2-level: Resource → Person).
///
/// Level 0: Resources sorted by name, displayed as "Name (N)" where N = people count
/// Level 1: People in each resource, sorted by display name
///
/// This model rebuilds on Full, EmergencyResource, and Family scope changes.
class EmergencyResourceModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    /// Node types in the tree
    enum class ItemType
    {
        Invalid,
        Resource,
        Person
    };
    Q_ENUM(ItemType)

    /// Custom roles for accessing item data
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        ItemTypeRole,
        ResourceIdRole  // For Person items: the parent resource's ID
    };
    Q_ENUM(Roles)

    explicit EmergencyResourceModel(DocumentManager* documentManager,
                                     ResponseArea area,
                                     QObject* parent = nullptr);
    ~EmergencyResourceModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // View-specific accessors
    QString idAt(const QModelIndex& index) const;
    ItemType itemTypeAt(const QModelIndex& index) const;
    QString resourceIdAt(const QModelIndex& index) const;

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void rebuild();
    void refreshFamilyDisplayText(const QString& familyId);
    void clearNodes();

    /// Internal tree node structure
    struct TreeNode
    {
        ItemType type;
        QString id;           // Resource ID or Person ID
        QString resourceId;   // For Person nodes: parent resource's ID
        QString displayText;
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;

        ~TreeNode()
        {
            qDeleteAll(children);
        }
    };

    TreeNode* nodeFromIndex(const QModelIndex& index) const;

    QList<TreeNode*> m_resourceNodes;  // Top-level nodes (owned)
    DocumentManager* m_documentManager;
    ResponseArea m_area;
};
