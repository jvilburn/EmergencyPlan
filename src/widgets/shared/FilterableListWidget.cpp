#include "FilterableListWidget.h"
#include "SearchField.h"
#include "AppStyles.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QSignalBlocker>

FilterableListWidget::FilterableListWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

FilterableListWidget::~FilterableListWidget() = default;

void FilterableListWidget::setupUi()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Search field
    m_searchField = new SearchField(this);
    layout->addWidget(m_searchField);

    // Filter button row
    QHBoxLayout* filterRow = new QHBoxLayout();
    filterRow->setContentsMargins(0, 0, 0, 0);

    m_addFilterButton = new QPushButton(tr("+ Add Filter"), this);
    m_addFilterButton->setStyleSheet(AppStyles::beveledButton());
    filterRow->addWidget(m_addFilterButton);
    filterRow->addStretch();

    layout->addLayout(filterRow);

    // List view
    m_listView = new QListView(this);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_listView, 1);
}

void FilterableListWidget::setupConnections()
{
    connect(m_searchField, &SearchField::searchTextChanged,
            this, &FilterableListWidget::onSearchTextChanged);
}

void FilterableListWidget::setModel(QAbstractItemModel* model)
{
    m_model = model;
    m_listView->setModel(model);

    if (m_listView->selectionModel())
    {
        connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &FilterableListWidget::onSelectionChanged);
    }
}

QAbstractItemModel* FilterableListWidget::model() const
{
    return m_model;
}

void FilterableListWidget::setIdRole(int role)
{
    m_idRole = role;
}

void FilterableListWidget::setSelectionMode(SelectionMode mode)
{
    if (m_selectionMode == mode)
    {
        return;
    }

    m_selectionMode = mode;

    if (mode == MultiSelect)
    {
        m_listView->setSelectionMode(QAbstractItemView::MultiSelection);
    }
    else
    {
        m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    }
}

FilterableListWidget::SelectionMode FilterableListWidget::selectionMode() const
{
    return m_selectionMode;
}

void FilterableListWidget::setSelectedIds(const QStringList& ids)
{
    QItemSelectionModel* selModel = m_listView->selectionModel();
    if (!m_model || !selModel)
    {
        return;
    }

    {
        const QSignalBlocker blocker(selModel);
        selModel->clearSelection();

        for (const QString& id : ids)
        {
            int row = rowForId(id);
            if (row >= 0)
            {
                QModelIndex index = m_model->index(row, 0);
                selModel->select(index, QItemSelectionModel::Select);
            }
        }
    }

    // Scroll to first selected item
    if (!ids.isEmpty())
    {
        int row = rowForId(ids.first());
        if (row >= 0)
        {
            m_listView->scrollTo(m_model->index(row, 0));
        }
    }

    emit selectionChanged(ids);
}

QStringList FilterableListWidget::selectedIds() const
{
    QStringList ids;

    if (!m_model || !m_listView->selectionModel())
    {
        return ids;
    }

    const QModelIndexList selectedIndexes = m_listView->selectionModel()->selectedRows();
    for (const QModelIndex& index : selectedIndexes)
    {
        ids.append(index.data(m_idRole).toString());
    }

    return ids;
}

QString FilterableListWidget::currentId() const
{
    if (!m_model)
    {
        return QString();
    }

    QModelIndex current = m_listView->currentIndex();
    if (!current.isValid())
    {
        return QString();
    }

    return current.data(m_idRole).toString();
}

SearchField* FilterableListWidget::searchField() const
{
    return m_searchField;
}

void FilterableListWidget::onSearchTextChanged(const QString& text)
{
    emit searchTextChanged(text);
}

void FilterableListWidget::onSelectionChanged()
{
    emit selectionChanged(selectedIds());
}

int FilterableListWidget::rowForId(const QString& id) const
{
    if (!m_model || id.isEmpty())
    {
        return -1;
    }

    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        QModelIndex index = m_model->index(row, 0);
        if (index.data(m_idRole).toString() == id)
        {
            return row;
        }
    }

    return -1;
}
