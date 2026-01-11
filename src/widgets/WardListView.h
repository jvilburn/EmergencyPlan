#pragma once

#include "MapHighlightProvider.h"

#include <QWidget>
#include <QModelIndex>
#include <QHash>

class ActionButtonsWidget;
class DocumentManager;
class FamilyEditPanel;
class FamilyTreeModel;
class Filter;
class QSplitter;
class QTreeView;
class SearchField;

/// Sidebar view for browsing ward families.
/// Uses QTreeView with deep tree structure:
///   - Family (expandable)
///     - Members (expandable to show details)
///     - Address
///     - Phone
///     - Actions (Edit/Delete buttons)
///
/// Implements MapHighlightProvider for the shared map:
///   - No highlighting (returns invalid colors)
///   - Opacity based on filter visibility
class WardListView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT

public:
    explicit WardListView(QWidget* parent = nullptr);

    /// Initialize with required dependencies. Creates owned Filter and Model.
    void setup(DocumentManager* documentManager);

    /// Get the currently selected family ID.
    QString selectedFamilyId() const;

    /// Select a family by ID and scroll to it.
    void setSelectedFamilyId(const QString& id);

    /// Get the list of currently visible (filtered) family IDs.
    QStringList visibleFamilyIdsList() const;

    /// Access the filter for external configuration (e.g., from filter dialogs).
    Filter* filter() const { return m_filter; }

    /// Returns true if an edit panel is currently open.
    bool isEditing() const;

    /// Close the edit panel if open (prompts for unsaved changes).
    /// Returns false if user cancelled.
    bool closeEditPanel();

    // MapHighlightProvider interface
    QColor familyColor(const QString& familyId) const override;
    qreal familyOpacity(const QString& familyId) const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    /// Emitted when a family is selected (for map centering).
    void familySelected(const QString& id);

    /// Emitted when the visible families change (for map marker emphasis).
    void visibleFamiliesChanged(const QStringList& ids);

    /// Emitted when edit is requested for a family.
    void editFamilyRequested(const QString& id);

    /// Emitted when delete is requested for a family.
    void deleteFamilyRequested(const QString& id);

private slots:
    void onSelectionChanged();
    void onSearchTextChanged(const QString& text);
    void onModelReset();
    void onItemExpanded(const QModelIndex& index);
    void onItemCollapsed(const QModelIndex& index);
    void onEditFamily(const QString& familyId);
    void onSaveFamily();
    void onCancelEdit();
    void onCloseEditPanel();

private:
    void setupUi();
    void attachActionButtons(const QModelIndex& familyIndex);
    void detachActionButtons(const QString& familyId);
    void updateEditHighlight();

    SearchField* m_searchField = nullptr;
    QTreeView* m_treeView = nullptr;
    FamilyTreeModel* m_model = nullptr;
    Filter* m_filter = nullptr;
    DocumentManager* m_documentManager = nullptr;

    // Track action button widgets per family
    QHash<QString, ActionButtonsWidget*> m_actionWidgets;

    // Edit panel integration
    QSplitter* m_splitter = nullptr;
    FamilyEditPanel* m_editPanel = nullptr;
    QString m_editingFamilyId;
};
