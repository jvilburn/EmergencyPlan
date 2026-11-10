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

void SelectionPreservingTreeView::mousePressEvent(QMouseEvent* event)
{
    QModelIndex clicked = indexAt(event->pos());
    if (clicked.isValid() && (clicked.flags() & Qt::ItemIsUserCheckable))
    {
        // Toggle the clicked item's checkbox
        QVariant checkData = clicked.data(Qt::CheckStateRole);
        Qt::CheckState newState = (checkData.toInt() == Qt::Checked)
            ? Qt::Unchecked : Qt::Checked;

        if ((event->modifiers() & Qt::ShiftModifier) && currentIndex().isValid())
        {
            // Shift+click: toggle range from current to clicked
            int start = qMin(currentIndex().row(), clicked.row());
            int end = qMax(currentIndex().row(), clicked.row());
            for (int row = start; row <= end; ++row)
            {
                QModelIndex rowIndex = model()->index(row, 0);
                if (rowIndex.flags() & Qt::ItemIsUserCheckable)
                {
                    model()->setData(rowIndex, newState, Qt::CheckStateRole);
                }
            }
        }
        else
        {
            // Plain click or Ctrl+click: toggle single item
            model()->setData(clicked, newState, Qt::CheckStateRole);
        }

        setCurrentIndex(clicked);
        return;
    }

    QTreeView::mousePressEvent(event);
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
    QModelIndex current = currentIndex();
    m_savedKey = current.isValid() ? std::optional<SelectionKey>(m_typedModel->selectionKeyAt(current)) : std::nullopt;

    m_expandedKeys.clear();
    collectExpandedKeys(QModelIndex(), m_expandedKeys);

    m_topLevelKeys.clear();
    for (int row = 0; row < model()->rowCount(); ++row)
    {
        m_topLevelKeys.append(m_typedModel->selectionKeyAt(model()->index(row, 0)));
    }

    QModelIndex top = indexAt(QPoint(1, 1));
    m_topKey = top.isValid() ? std::optional<SelectionKey>(m_typedModel->selectionKeyAt(top)) : std::nullopt;
}

void SelectionPreservingTreeView::onModelReset()
{
    // Parents come before their children, so each is expanded before what is under it.
    for (const SelectionKey& key : std::as_const(m_expandedKeys))
    {
        QModelIndex index = findIndex(key);
        if (index.isValid())
        {
            expand(index);
        }
    }
    if (m_expandsNewTopLevelRows)
    {
        for (int row = 0; row < model()->rowCount(); ++row)
        {
            QModelIndex index = model()->index(row, 0);
            if (!m_topLevelKeys.contains(m_typedModel->selectionKeyAt(index)))
            {
                expand(index);
            }
        }
    }

    if (m_savedKey.has_value())
    {
        QModelIndex index = findIndex(*m_savedKey);
        if (index.isValid())
        {
            setCurrentIndex(index);
        }
    }

    if (m_topKey.has_value())
    {
        QModelIndex index = findIndex(*m_topKey);
        if (index.isValid())
        {
            scrollTo(index, QAbstractItemView::PositionAtTop);
        }
    }

    m_savedKey = std::nullopt;
    m_expandedKeys.clear();
    m_topKey = std::nullopt;
    m_topLevelKeys.clear();
}

void SelectionPreservingTreeView::collectExpandedKeys(const QModelIndex& parent, QList<SelectionKey>& keys) const
{
    int rowCount = model()->rowCount(parent);
    for (int row = 0; row < rowCount; ++row)
    {
        QModelIndex index = model()->index(row, 0, parent);
        if (isExpanded(index))
        {
            keys.append(m_typedModel->selectionKeyAt(index));
            collectExpandedKeys(index, keys);
        }
    }
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
