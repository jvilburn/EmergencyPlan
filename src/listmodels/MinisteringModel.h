#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QString>

#include "DocumentChange.h"
#include "ItemType.h"

class DocumentManager;

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
class MinisteringModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    /// Custom roles for accessing item data
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NodeTypeRole,
        SecondaryIdRole  // For ContactDetail: the parent person/family ID
    };
    Q_ENUM(Roles)

    explicit MinisteringModel(DocumentManager* documentManager,
                               bool isEQ,
                               QObject* parent = nullptr);
    ~MinisteringModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;

    // View-specific accessors
    QString idAt(const QModelIndex& index) const;
    /// Returns the node type at the given index, or ItemType::Invalid for invalid indexes.
    ItemType nodeTypeAt(const QModelIndex& index) const;
    QString companionshipIdAt(const QModelIndex& index) const;

    // Lazy loading for contact details
    void loadContactDetails(const QModelIndex& index);
    bool hasContactsLoaded(const QModelIndex& index) const;

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
        QString id;
        QString displayText;
        QString secondaryId;  // For ContactDetail: person/family ID
        TreeNode* parent = nullptr;
        QList<TreeNode*> children;
        bool contactsLoaded = false;

        ~TreeNode()
        {
            qDeleteAll(children);
        }
    };

    TreeNode* nodeFromIndex(const QModelIndex& index) const;
    void addMinistersSection(TreeNode* companionshipNode, const QString& groupId);
    void addMinisteredSection(TreeNode* companionshipNode, const QString& groupId);

    QList<TreeNode*> m_districtNodes;  // Top-level nodes (owned)
    DocumentManager* m_documentManager;
    bool m_isEQ;
};
