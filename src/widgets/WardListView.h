#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>
#include <QModelIndex>
#include <QHash>

class ActionButtonsWidget;
class DocumentManager;
class FamilyTreeModel;
class Filter;
class SelectionPreservingTreeView;
class SearchField;

/// Tree view for ward family list.
/// Takes model pointer - parent creates model with Filter.
class WardListView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit WardListView(FamilyTreeModel* model,
                          Filter* filter,
                          DocumentManager* documentManager,
                          QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

    QString selectedFamilyId() const;
    void setSelectedFamilyId(const QString& id);
    QStringList visibleFamilyIdsList() const;

    Filter* filter() const { return m_filter; }

signals:
    void highlightChanged();
    void visibleFamiliesChanged(const QStringList& familyIds);
    void editFamilyRequested(const QString& familyId);
    void deleteFamilyRequested(const QString& familyId);

private slots:
    void onSelectionChanged();
    void onSearchTextChanged(const QString& text);
    void onModelReset();
    void onRowsRemoved(const QModelIndex& parent, int first, int last);
    void onRowsInserted(const QModelIndex& parent, int first, int last);
    void onItemExpanded(const QModelIndex& index);
    void onItemCollapsed(const QModelIndex& index);

private:
    void attachActionButtons(const QModelIndex& familyIndex);
    void detachActionButtons(const QString& familyId);

    DocumentManager* m_documentManager;
    FamilyTreeModel* m_model;
    Filter* m_filter;
    SelectionPreservingTreeView* m_treeView;
    SearchField* m_searchField;
    QHash<QString, ActionButtonsWidget*> m_actionWidgets;
};
