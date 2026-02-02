#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>
#include <QModelIndex>
#include <QHash>

class ActionButtonsWidget;
class DocumentManager;
class FamilyTreeModel;
class Filter;
class FilterBar;
class SelectionPreservingTreeView;

/// Tree view for ward family list.
/// Owns FilterBar (which owns Filter) and FamilyTreeModel internally.
class WardListView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit WardListView(DocumentManager* documentManager,
                          QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

    QString selectedFamilyId() const;
    void setSelectedFamilyId(const QString& id);
    QStringList visibleFamilyIdsList() const;

    Filter* filter() const;

signals:
    void highlightChanged();
    void visibleFamiliesChanged(const QStringList& familyIds);
    void editFamilyRequested(const QString& familyId);
    void deleteFamilyRequested(const QString& familyId);

private slots:
    void onSelectionChanged();
    void onModelReset();
    void onRowsRemoved(const QModelIndex& parent, int first, int last);
    void onRowsInserted(const QModelIndex& parent, int first, int last);
    void onItemExpanded(const QModelIndex& index);
    void onItemCollapsed(const QModelIndex& index);

private:
    void attachActionButtons(const QModelIndex& familyIndex);
    void detachActionButtons(const QString& familyId);

    DocumentManager* m_documentManager;
    FilterBar* m_filterBar;
    FamilyTreeModel* m_model;
    SelectionPreservingTreeView* m_treeView;
    QHash<QString, ActionButtonsWidget*> m_actionWidgets;
};
