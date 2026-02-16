#pragma once

#include "BaseTreeModel.h"
#include "DocumentChange.h"
#include "ItemType.h"

#include <QList>
#include <QString>

class DocumentManager;
class Filter;

/// Tree model showing persons with expandable contact details.
/// For use in person selection dialogs.
///
/// Structure:
///   - Person (level 0) - expandable, shows display name
///     - ContactDetail (level 1) - phone, alt phone, email, address
class PersonTreeModel : public BaseTreeModel
{
    Q_OBJECT

public:
    enum Roles
    {
        PersonIdRole = Qt::UserRole + 1,
        FamilyIdRole
    };
    Q_ENUM(Roles)

    explicit PersonTreeModel(DocumentManager* documentManager,
                             Filter* filter,
                             QObject* parent = nullptr);
    ~PersonTreeModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    SelectionKey selectionKeyAt(const QModelIndex& index) const override;

    // Person-specific
    std::optional<PersonId> personIdAt(const QModelIndex& index) const;
    std::optional<FamilyId> familyIdAt(const QModelIndex& index) const;
    QModelIndex indexForPersonId(const PersonId& personId) const;

public slots:
    void rebuild();

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void clearNodes();
    void buildPersonNode(int personIndex);

    /// Internal tree node structure
    struct TreeNode
    {
        ItemType type;
        PersonId personId;
        FamilyId familyId;
        QString displayText;
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;

        ~TreeNode() { qDeleteAll(children); }
    };

    TreeNode* nodeFromIndex(const QModelIndex& index) const;

    // Sorted list of (personId, familyId) pairs
    QList<QPair<PersonId, FamilyId>> m_personData;
    QList<TreeNode*> m_personNodes;  // Top-level nodes (owned)
    DocumentManager* m_documentManager;
    Filter* m_filter;
};
