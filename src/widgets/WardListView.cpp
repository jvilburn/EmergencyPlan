#include "WardListView.h"
#include "ActionButtonsWidget.h"
#include "DocumentManager.h"
#include "Family.h"
#include "FamilyTreeModel.h"
#include "Filter.h"
#include "SearchField.h"

#include <QHeaderView>
#include <QTreeView>
#include <QVBoxLayout>

WardListView::WardListView(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void WardListView::setupUi()
{
    setMinimumWidth(250);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Search field
    m_searchField = new SearchField(this);
    m_searchField->setPlaceholderText(tr("Search families..."));
    layout->addWidget(m_searchField);

    // Tree view
    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setAnimated(true);
    m_treeView->setExpandsOnDoubleClick(false);  // We handle expand on single click
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->header()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_treeView, 1);

    connect(m_searchField, &SearchField::searchTextChanged,
            this, &WardListView::onSearchTextChanged);

    connect(m_treeView, &QTreeView::expanded,
            this, &WardListView::onItemExpanded);

    connect(m_treeView, &QTreeView::collapsed,
            this, &WardListView::onItemCollapsed);
}

void WardListView::setup(DocumentManager* documentManager)
{
    m_documentManager = documentManager;

    // Create owned filter
    m_filter = new Filter(this);

    // Create tree model with document manager and filter
    m_model = new FamilyTreeModel(documentManager, m_filter, this);

    m_treeView->setModel(m_model);

    // Connect selection changes
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &WardListView::onSelectionChanged);

    // Connect model reset to handle expanded state
    connect(m_model, &QAbstractItemModel::modelReset,
            this, &WardListView::onModelReset);

    // Connect rowsRemoved to clean up action widgets when children are rebuilt
    connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, &WardListView::onRowsRemoved);

    // Connect rowsInserted to re-attach action widgets for expanded families
    connect(m_model, &QAbstractItemModel::rowsInserted,
            this, &WardListView::onRowsInserted);

    // Emit initial visible families
    emit visibleFamiliesChanged(visibleFamilyIdsList());
}

QString WardListView::selectedFamilyId() const
{
    if (!m_treeView || !m_model)
    {
        return QString();
    }

    QModelIndex current = m_treeView->currentIndex();
    if (!current.isValid())
    {
        return QString();
    }

    return m_model->familyIdAt(current);
}

void WardListView::setSelectedFamilyId(const QString& id)
{
    if (!m_model || !m_treeView)
    {
        return;
    }

    QModelIndex familyIndex = m_model->indexForFamilyId(id);
    if (familyIndex.isValid())
    {
        m_treeView->setCurrentIndex(familyIndex);
        m_treeView->scrollTo(familyIndex);
    }
}

QStringList WardListView::visibleFamilyIdsList() const
{
    if (!m_model)
    {
        return {};
    }
    return m_model->familyIds();
}

// FamilyMarkerProvider interface implementation

HighlightInfo WardListView::highlightInfo() const
{
    HighlightInfo info;

    // Highlight selected family (if any)
    QString selected = selectedFamilyId();
    if (!selected.isEmpty())
    {
        info.highlightedFamilyIds.insert(selected);
    }

    // WardListView has no contact point concept
    return info;
}

QSet<QString> WardListView::visibleFamilyIds() const
{
    QStringList list = visibleFamilyIdsList();
    return QSet<QString>(list.begin(), list.end());
}

void WardListView::onSelectionChanged()
{
    QString id = selectedFamilyId();
    if (!id.isEmpty())
    {
        emit highlightChanged();

        // Expand the selected family if it's a family row
        QModelIndex current = m_treeView->currentIndex();
        if (current.isValid())
        {
            FamilyTreeModel::RowType type = m_model->rowTypeAt(current);
            if (type == FamilyTreeModel::RowType::Family && !m_treeView->isExpanded(current))
            {
                m_treeView->expand(current);
            }
        }
    }
}

void WardListView::onSearchTextChanged(const QString& text)
{
    if (m_filter)
    {
        m_filter->setSearchText(text);
    }
}

void WardListView::onModelReset()
{
    // Clear all action widgets - they were deleted by the model reset
    m_actionWidgets.clear();

    emit visibleFamiliesChanged(visibleFamilyIdsList());
}

void WardListView::onRowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(first)
    Q_UNUSED(last)

    // When a family's children are removed (during surgical update),
    // the action widget is deleted by Qt. Clean up our tracking.
    if (parent.isValid())
    {
        FamilyTreeModel::RowType type = m_model->rowTypeAt(parent);
        if (type == FamilyTreeModel::RowType::Family)
        {
            QString familyId = m_model->familyIdAt(parent);
            m_actionWidgets.remove(familyId);
        }
    }
}

void WardListView::onRowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(first)
    Q_UNUSED(last)

    // When children are inserted under an expanded family, re-attach action buttons
    if (parent.isValid())
    {
        FamilyTreeModel::RowType type = m_model->rowTypeAt(parent);
        if (type == FamilyTreeModel::RowType::Family && m_treeView->isExpanded(parent))
        {
            attachActionButtons(parent);
        }
    }
}

void WardListView::onItemExpanded(const QModelIndex& index)
{
    FamilyTreeModel::RowType type = m_model->rowTypeAt(index);

    if (type == FamilyTreeModel::RowType::Family)
    {
        // Attach action buttons when family is expanded
        attachActionButtons(index);
    }
}

void WardListView::onItemCollapsed(const QModelIndex& index)
{
    FamilyTreeModel::RowType type = m_model->rowTypeAt(index);

    if (type == FamilyTreeModel::RowType::Family)
    {
        QString familyId = m_model->familyIdAt(index);
        detachActionButtons(familyId);
    }
}

void WardListView::attachActionButtons(const QModelIndex& familyIndex)
{
    QString familyId = m_model->familyIdAt(familyIndex);
    if (familyId.isEmpty())
    {
        return;
    }

    // Already have buttons for this family?
    if (m_actionWidgets.contains(familyId))
    {
        return;
    }

    // Find the Actions row (last child of family)
    int childCount = m_model->rowCount(familyIndex);
    for (int i = 0; i < childCount; ++i)
    {
        QModelIndex childIndex = m_model->index(i, 0, familyIndex);
        FamilyTreeModel::RowType childType = m_model->rowTypeAt(childIndex);

        if (childType == FamilyTreeModel::RowType::Actions)
        {
            ActionButtonsWidget* widget = new ActionButtonsWidget(familyId, m_treeView);

            connect(widget, &ActionButtonsWidget::editRequested,
                    this, &WardListView::editFamilyRequested);
            connect(widget, &ActionButtonsWidget::deleteRequested,
                    this, &WardListView::deleteFamilyRequested);

            m_treeView->setIndexWidget(childIndex, widget);
            m_actionWidgets[familyId] = widget;
            break;
        }
    }
}

void WardListView::detachActionButtons(const QString& familyId)
{
    if (!m_actionWidgets.contains(familyId))
    {
        return;
    }

    // The widget will be deleted by setIndexWidget(nullptr)
    // We just need to remove from our tracking hash
    QModelIndex familyIndex = m_model->indexForFamilyId(familyId);
    if (familyIndex.isValid())
    {
        int childCount = m_model->rowCount(familyIndex);
        for (int i = 0; i < childCount; ++i)
        {
            QModelIndex childIndex = m_model->index(i, 0, familyIndex);
            FamilyTreeModel::RowType childType = m_model->rowTypeAt(childIndex);

            if (childType == FamilyTreeModel::RowType::Actions)
            {
                m_treeView->setIndexWidget(childIndex, nullptr);
                break;
            }
        }
    }

    m_actionWidgets.remove(familyId);
}
