#pragma once

#include "FamilyMarkerProvider.h"
#include "MinisteringModel.h"

#include <QWidget>
#include <QHash>
#include <QSet>

class DocumentManager;
class MinisteringGroup;
class UnassignedMinisteringModel;
class QTabBar;
class QTreeView;

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
    void onSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    void onUnassignedSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    void onTreeExpanded(const QModelIndex& index);
    void onUnassignedTreeExpanded(const QModelIndex& index);
    void onUnassignedTreeCollapsed(const QModelIndex& index);
    void onUnassignedModelReset();

private:
    using NodeType = MinisteringModel::NodeType;

    void setupUi();
    void updateUnassignedVisibility();

    // For RS: Get family IDs containing the given person IDs
    QSet<QString> familyIdsForPersons(const QSet<QString>& personIds) const;

    // Get unassigned family IDs (EQ) or person IDs (RS)
    QSet<QString> unassignedFamilyIds() const;
    QSet<QString> unassignedSisterIds() const;

    DocumentManager* m_documentManager;

    // Models
    MinisteringModel* m_eqModel = nullptr;
    MinisteringModel* m_rsModel = nullptr;
    UnassignedMinisteringModel* m_eqUnassignedModel = nullptr;
    UnassignedMinisteringModel* m_rsUnassignedModel = nullptr;

    // UI
    QTabBar* m_orgTabs = nullptr;
    QTreeView* m_eqTree = nullptr;
    QTreeView* m_rsTree = nullptr;
    QTreeView* m_eqUnassignedTree = nullptr;
    QTreeView* m_rsUnassignedTree = nullptr;

    // State
    bool m_isEQ = true;

    // EQ selection state (ID and type for highlightInfo)
    QString m_eqSelectedId;
    NodeType m_eqSelectedType = NodeType::Invalid;

    // RS selection state
    QString m_rsSelectedId;
    NodeType m_rsSelectedType = NodeType::Invalid;
};
