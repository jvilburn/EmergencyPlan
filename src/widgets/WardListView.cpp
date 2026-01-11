#include "WardListView.h"
#include "ActionButtonsWidget.h"
#include "DocumentManager.h"
#include "Family.h"
#include "FamilyCommands.h"
#include "FamilyEditPanel.h"
#include "FamilyTreeModel.h"
#include "Filter.h"
#include "SearchField.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QSplitter>
#include <QTreeView>
#include <QVBoxLayout>

WardListView::WardListView(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void WardListView::setupUi()
{
    // Create splitter for list and edit panel
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Container for list content
    QWidget* listContainer = new QWidget(m_splitter);
    QVBoxLayout* listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(4);

    // Search field
    m_searchField = new SearchField(listContainer);
    m_searchField->setPlaceholderText(tr("Search families..."));
    listLayout->addWidget(m_searchField);

    // Tree view
    m_treeView = new QTreeView(listContainer);
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setAnimated(true);
    m_treeView->setExpandsOnDoubleClick(false);  // We handle expand on single click
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->header()->setSectionResizeMode(QHeaderView::Stretch);
    listLayout->addWidget(m_treeView, 1);

    m_splitter->addWidget(listContainer);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_splitter);

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

    // Create edit panel (hidden by default)
    m_editPanel = new FamilyEditPanel(documentManager, m_splitter);
    m_editPanel->hide();
    m_splitter->addWidget(m_editPanel);

    // Connect edit panel signals
    connect(m_editPanel, &FamilyEditPanel::saveRequested,
            this, &WardListView::onSaveFamily);
    connect(m_editPanel, &FamilyEditPanel::cancelRequested,
            this, &WardListView::onCancelEdit);
    connect(m_editPanel, &FamilyEditPanel::closeRequested,
            this, &WardListView::onCloseEditPanel);

    // Connect edit family request
    connect(this, &WardListView::editFamilyRequested,
            this, &WardListView::onEditFamily);

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

// MapHighlightProvider interface implementation

QColor WardListView::familyColor(const QString& /*familyId*/) const
{
    // WardListView doesn't highlight - it only filters visibility
    return QColor();  // Invalid color means no highlighting
}

qreal WardListView::familyOpacity(const QString& familyId) const
{
    // Visible families are fully opaque, filtered-out would be dimmed
    // But since visibleFamilyIds() returns only visible ones, this is always 1.0
    Q_UNUSED(familyId);
    return 1.0;
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
        emit familySelected(id);

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

void WardListView::onEditFamily(const QString& familyId)
{
    // Close existing panel if editing different family
    if (isEditing() && m_editingFamilyId != familyId)
    {
        if (!closeEditPanel())
        {
            return;  // User cancelled
        }
    }

    // Get family from document
    const Document& doc = m_documentManager->document();
    if (!doc.families().contains(familyId))
    {
        return;
    }

    const Family& family = doc.families().value(familyId);
    m_editingFamilyId = familyId;
    m_editPanel->setFamily(family);
    m_editPanel->show();

    // Set splitter sizes (list:panel = 1:1)
    m_splitter->setSizes({m_splitter->width() / 2, m_splitter->width() / 2});
}

void WardListView::onSaveFamily()
{
    if (!isEditing())
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    if (!doc.families().contains(m_editingFamilyId))
    {
        // Family was deleted externally
        m_editingFamilyId.clear();
        m_editPanel->hide();
        return;
    }

    Family oldFamily = doc.families().value(m_editingFamilyId);
    Family newFamily = m_editPanel->family();

    m_documentManager->executeCommand(
        std::make_unique<UpdateFamilyCommand>(oldFamily, newFamily));

    m_editingFamilyId.clear();
    m_editPanel->hide();
}

void WardListView::onCancelEdit()
{
    m_editingFamilyId.clear();
    m_editPanel->hide();
}

void WardListView::onCloseEditPanel()
{
    closeEditPanel();
}

bool WardListView::isEditing() const
{
    return !m_editingFamilyId.isEmpty() && m_editPanel->isVisible();
}

bool WardListView::closeEditPanel()
{
    if (!isEditing())
    {
        return true;
    }

    if (m_editPanel->isDirty())
    {
        const Family& original = m_documentManager->document().families().value(m_editingFamilyId);
        QString familyName = original.surname();
        if (familyName.isEmpty())
        {
            familyName = tr("this family");
        }

        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Save changes to %1?").arg(familyName),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (result == QMessageBox::Save)
        {
            onSaveFamily();
            return true;
        }
        else if (result == QMessageBox::Cancel)
        {
            return false;
        }
        // Discard - fall through
    }

    m_editingFamilyId.clear();
    m_editPanel->hide();
    return true;
}
