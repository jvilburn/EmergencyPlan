#include "SelectableTreeView.h"

#include <QTreeView>
#include <QItemSelectionModel>

SelectableTreeView::SelectableTreeView(QTreeView* tree, QWidget* parent)
    : QWidget(parent)
{
    connect(tree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &SelectableTreeView::onSelectionChanged);
}

void SelectableTreeView::onSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    if (!current.isValid())
    {
        m_selectedType = ItemType::Invalid;
        m_selectedId.clear();
        emit highlightChanged();
        return;
    }

    QModelIndex nodeToUse = current;
    if (itemTypeAt(current) == ItemType::ContactDetail)
    {
        nodeToUse = current.parent();
    }

    m_selectedType = itemTypeAt(nodeToUse);
    m_selectedId = idAt(nodeToUse);
    emit highlightChanged();
}

HighlightInfo SelectableTreeView::highlightInfo() const
{
    if (m_selectedType == ItemType::Invalid || m_selectedId.isEmpty())
    {
        return {};
    }
    return computeHighlight();
}

QSet<QString> SelectableTreeView::visibleFamilyIds() const
{
    return {};  // Show all by default
}
