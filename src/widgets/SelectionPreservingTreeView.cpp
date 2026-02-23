#include "SelectionPreservingTreeView.h"
#include "ItemType.h"
#include "BaseTreeModel.h"
#include "SculptedItemDelegate.h"

#include <QItemSelectionModel>

SelectionPreservingTreeView::SelectionPreservingTreeView(BaseTreeModel* model, QWidget* parent)
    : QTreeView(parent)
    , m_typedModel(model)
{
    // BaseTreeModel inherits from QAbstractItemModel, so no cast needed
    QTreeView::setModel(model);

    // Sculpted delegate for button-like item appearance
    setItemDelegate(new SculptedItemDelegate(model, this));

    connect(model, &QAbstractItemModel::modelAboutToBeReset,
            this, &SelectionPreservingTreeView::onModelAboutToBeReset);
    connect(model, &QAbstractItemModel::modelReset,
            this, &SelectionPreservingTreeView::onModelReset);
    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &SelectionPreservingTreeView::onCurrentChanged);
}

void SelectionPreservingTreeView::clearSelection()
{
    QTreeView::clearSelection();
    setCurrentIndex(QModelIndex());
    emit selectionChanged();
}

void SelectionPreservingTreeView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        clearSelection();
        return;
    }
    QTreeView::keyPressEvent(event);
}

void SelectionPreservingTreeView::onCurrentChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    if (current.isValid()
        && m_typedModel->itemTypeAt(current) == ItemType::ContactDetail
        && current.parent().isValid())
    {
        // Redirect detail row clicks to parent
        setCurrentIndex(current.parent());
    }
    else
    {
        emit selectionChanged();
    }
}

void SelectionPreservingTreeView::onModelAboutToBeReset()
{
    // Save current selection before model clears
    QModelIndex current = currentIndex();
    if (current.isValid())
    {
        m_savedKey = m_typedModel->selectionKeyAt(current);
    }
    else
    {
        m_savedKey = std::nullopt;
    }
}

void SelectionPreservingTreeView::onModelReset()
{
    // Try to restore selection after model rebuild
    if (m_savedKey.has_value())
    {
        QModelIndex index = findIndex(*m_savedKey);
        if (index.isValid())
        {
            setCurrentIndex(index);
        }
    }

    // Clear saved state
    m_savedKey = std::nullopt;
}

QModelIndex SelectionPreservingTreeView::findIndex(const SelectionKey& key) const
{
    if (!model())
    {
        return QModelIndex();
    }
    return findIndexRecursive(QModelIndex(), key);
}

QModelIndex SelectionPreservingTreeView::findIndexRecursive(const QModelIndex& parent, const SelectionKey& key) const
{
    int rowCount = model()->rowCount(parent);
    for (int row = 0; row < rowCount; ++row)
    {
        QModelIndex index = model()->index(row, 0, parent);
        if (m_typedModel->selectionKeyAt(index) == key)
        {
            return index;
        }

        // Search children
        QModelIndex found = findIndexRecursive(index, key);
        if (found.isValid())
        {
            return found;
        }
    }
    return QModelIndex();
}
