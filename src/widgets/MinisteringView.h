#pragma once

#include "FamilyMarkerProvider.h"
#include "DocumentChange.h"

#include <QWidget>
#include <QHash>
#include <QSet>

class DocumentManager;
class MinisteringGroup;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;

/// Sidebar view for reviewing ministering assignments with geographic visualization.
/// Shows EQ or RS ministering districts and companionships in a tree structure.
/// Implements FamilyMarkerProvider for the shared map:
///   - Colors families by selected companionship/district
///   - Dims unselected families
class MinisteringView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit MinisteringView(DocumentManager* docManager, QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    /// Emitted when highlighting changes (for map update)
    void highlightChanged();

private slots:
    void onOrgToggled(int id);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onDocumentChanged(const DocumentChange& change);
    void onTreeItemExpanded(QTreeWidgetItem* item);
    void onUnassignedTreeItemExpanded(QTreeWidgetItem* item);
    void onUnassignedTreeItemCollapsed(QTreeWidgetItem* item);

private:
    void setupUi();
    void rebuildTreeImpl(QTreeWidget* tree, bool isEQ);
    void rebuildUnassignedTreeImpl(QTreeWidget* tree, bool isEQ);
    void updateUnassignedVisibility();
    void handleTreeItemClicked(QTreeWidgetItem* item, bool isEQ, bool isUnassigned);
    void clearSelection(bool isEQ);
    void populateContactInfo(QTreeWidgetItem* item);
    void addMinistersSection(QTreeWidgetItem* companionshipItem, const MinisteringGroup& group, bool isEQ);
    void addMinisteredSection(QTreeWidgetItem* companionshipItem, const MinisteringGroup& group, bool isEQ);

    // For RS: Get family IDs containing the given person IDs
    QSet<QString> familyIdsForPersons(const QSet<QString>& personIds) const;

    // Get unassigned family IDs (EQ) or person IDs (RS)
    QSet<QString> unassignedFamilyIds() const;
    QSet<QString> unassignedSisterIds() const;

    // Constants for tree item data roles
    static constexpr int IdRole = Qt::UserRole;
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SecondaryIdRole = Qt::UserRole + 2;  // For contact: person/family ID
    enum class ItemType {
        District,
        Companionship,
        SectionHeader,     // "Ministers", "Families", "Sisters"
        Minister,          // Individual minister person
        MinisteredFamily,  // EQ: family being ministered to
        MinisteredSister,  // RS: sister being ministered to
        ContactDetail,     // Phone, email, address line
        UnassignedHeader   // Root "Unassigned" item
    };

    DocumentManager* m_documentManager;

    // UI
    QTabBar* m_orgTabs = nullptr;
    QTreeWidget* m_eqTree = nullptr;
    QTreeWidget* m_rsTree = nullptr;
    QTreeWidget* m_eqUnassignedTree = nullptr;
    QTreeWidget* m_rsUnassignedTree = nullptr;

    // State
    bool m_isEQ = true;

    // EQ selection state
    QString m_eqSelectedId;
    ItemType m_eqSelectedType = ItemType::District;
    QTreeWidgetItem* m_eqSelectedItem = nullptr;

    // RS selection state
    QString m_rsSelectedId;
    ItemType m_rsSelectedType = ItemType::District;
    QTreeWidgetItem* m_rsSelectedItem = nullptr;
};
