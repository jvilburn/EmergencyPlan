#pragma once

#include "FamilyMarkerProvider.h"
#include "ItemType.h"

#include <QWidget>

class QTreeView;
class QAbstractItemModel;

/// Base class for tree views with selection caching and parent redirect.
/// Subclasses implement model-specific highlight computation.
class SelectableTreeView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit SelectableTreeView(QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override final;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

protected:
    // Subclasses must implement
    virtual ItemType itemTypeAt(const QModelIndex& index) const = 0;
    virtual QString idAt(const QModelIndex& index) const = 0;
    virtual HighlightInfo computeHighlight() const = 0;

    // Subclasses call this to set up the tree
    void initTree(QTreeView* tree);

    // Selection state (accessible to subclasses for computeHighlight)
    ItemType m_selectedType = ItemType::Invalid;
    QString m_selectedId;

    QTreeView* m_tree = nullptr;

private slots:
    void onSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
};
