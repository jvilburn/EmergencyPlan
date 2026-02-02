#pragma once

#include "BaseTreeModel.h"
#include "DocumentChange.h"

#include <QList>
#include <QString>

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
class MinisteringModel : public BaseTreeModel
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
                               MinisteringOrg org,
                               QObject* parent = nullptr);
    ~MinisteringModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;

    // BaseTreeModel interface
    ItemType itemTypeAt(const QModelIndex& index) const override;
    QString selectionKeyAt(const QModelIndex& index) const override;
    QString idAt(const QModelIndex& index) const override;

    /// Returns family associations for the given index.
    /// Used by views to compute map highlights.
    FamilyAssociation relatedFamiliesAt(const QModelIndex& index) const;

    // View-specific accessors
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
    bool isEQ() const { return m_org == MinisteringOrg::EldersQuorum; }
    QSet<QString> familyIdsForPersons(const QSet<QString>& personIds) const;

    QList<TreeNode*> m_districtNodes;  // Top-level nodes (owned)
    DocumentManager* m_documentManager;
    MinisteringOrg m_org;
};
