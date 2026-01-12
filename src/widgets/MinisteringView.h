#pragma once

#include "MapHighlightProvider.h"
#include "DocumentChange.h"

#include <QWidget>
#include <QHash>
#include <QSet>
#include <QColor>

class DocumentManager;
class QButtonGroup;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

/// Sidebar view for reviewing ministering assignments with geographic visualization.
/// Shows EQ or RS ministering districts and companionships in a tree structure.
/// Implements MapHighlightProvider for the shared map:
///   - Colors families by selected companionship/district
///   - Dims unselected families
class MinisteringView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT

public:
    explicit MinisteringView(DocumentManager* docManager, QWidget* parent = nullptr);

    // MapHighlightProvider interface
    QColor familyColor(const QString& familyId) const override;
    qreal familyOpacity(const QString& familyId) const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    /// Emitted when highlighting changes (for map update)
    void highlightChanged();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onOrgToggled(int id);
    void onUnassignedClicked();
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onDocumentChanged(const DocumentChange& change);

private:
    void setupUi();
    void rebuildTree();
    void regenerateColors();
    void updateUnassignedLabel();
    void clearSelection();

    // Get family IDs for the current selection
    QSet<QString> selectedFamilyIds() const;

    // For RS: Get family IDs containing the given person IDs
    QSet<QString> familyIdsForPersons(const QSet<QString>& personIds) const;

    // Get unassigned family IDs (EQ) or person IDs (RS)
    QSet<QString> unassignedFamilyIds() const;
    QSet<QString> unassignedSisterIds() const;

    DocumentManager* m_documentManager;

    // UI
    QButtonGroup* m_orgToggle = nullptr;
    QLabel* m_unassignedLabel = nullptr;
    QTreeWidget* m_tree = nullptr;

    // State
    bool m_isEQ = true;
    QSet<QString> m_selectedDistrictIds;
    QSet<QString> m_selectedCompanionshipIds;
    bool m_unassignedSelected = false;
    QHash<QString, QColor> m_colorMap;  // ID -> color (for companionships and districts)

    // Constants for tree item data roles
    static constexpr int IdRole = Qt::UserRole;
    static constexpr int TypeRole = Qt::UserRole + 1;
    enum class ItemType { District, Companionship };
};
