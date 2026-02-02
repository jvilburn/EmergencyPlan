#include "WardListDialog.h"
#include "FilterBar.h"
#include "FamilyTreeModel.h"
#include "PersonTreeModel.h"
#include "ItemType.h"
#include "MapWidget.h"
#include "DocumentManager.h"
#include "Document.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QDialogButtonBox>
#include <QTreeView>
#include <QItemSelectionModel>

WardListDialog::WardListDialog(DocumentManager* documentManager,
                               Mode mode,
                               QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_documentManager(documentManager)
{
    setupUi();
}

void WardListDialog::setupUi()
{
    // Set dialog title based on mode
    if (m_mode == FamilyMode)
    {
        setWindowTitle(tr("Select Family"));
    }
    else
    {
        setWindowTitle(tr("Select Person"));
    }

    resize(1000, 700);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create FilterBar (owns Filter internally)
    m_filterBar = new FilterBar(m_documentManager, this);
    mainLayout->addWidget(m_filterBar);

    // Create splitter with tree and map
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Create tree view
    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setIndentation(16);

    // Create appropriate model based on mode
    if (m_mode == FamilyMode)
    {
        m_familyModel = new FamilyTreeModel(m_documentManager, m_filterBar->filter(), this);
        m_treeView->setModel(m_familyModel);
    }
    else
    {
        m_personModel = new PersonTreeModel(m_documentManager, m_filterBar->filter(), this);
        m_treeView->setModel(m_personModel);
    }

    // Create map widget
    m_mapWidget = new MapWidget(m_documentManager, this);
    m_mapWidget->setMinimumWidth(400);

    // Add to splitter
    m_splitter->addWidget(m_treeView);
    m_splitter->addWidget(m_mapWidget);
    m_splitter->setSizes({350, 650});
    m_splitter->setStretchFactor(0, 0);  // Tree doesn't stretch
    m_splitter->setStretchFactor(1, 1);  // Map stretches

    mainLayout->addWidget(m_splitter, 1);

    // Button box
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    // Set this dialog as the highlight provider for the map
    m_mapWidget->setMarkerProvider(this);

    // Connections
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &WardListDialog::onSelectionChanged);
    connect(m_mapWidget, &MapWidget::familyClicked,
            this, &WardListDialog::onMapFamilyClicked);
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    // Fit map to show all families
    m_mapWidget->fitAllFamilies();
}

void WardListDialog::setSelectionMode(SelectionMode mode)
{
    if (mode == MultiSelect)
    {
        m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    else
    {
        m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    }
}

void WardListDialog::setPreselectedIds(const QStringList& ids)
{
    QItemSelectionModel* selModel = m_treeView->selectionModel();
    selModel->clearSelection();

    for (const QString& id : ids)
    {
        QModelIndex index;
        if (m_mode == FamilyMode && m_familyModel)
        {
            index = m_familyModel->indexForFamilyId(id);
        }
        else if (m_mode == PersonMode && m_personModel)
        {
            index = m_personModel->indexForPersonId(id);
        }

        if (index.isValid())
        {
            selModel->select(index, QItemSelectionModel::Select);
            // Scroll to first selected item
            if (ids.indexOf(id) == 0)
            {
                m_treeView->scrollTo(index);
            }
        }
    }
}

QStringList WardListDialog::selectedIds() const
{
    QStringList ids;
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();

    for (const QModelIndex& index : selected)
    {
        if (m_mode == FamilyMode && m_familyModel)
        {
            // Only include Family-type rows (not members or details)
            if (m_familyModel->itemTypeAt(index) == ItemType::Family)
            {
                QString id = m_familyModel->familyIdAt(index);
                if (!id.isEmpty() && !ids.contains(id))
                {
                    ids.append(id);
                }
            }
        }
        else if (m_mode == PersonMode && m_personModel)
        {
            // Only include Person-type rows (not contact details)
            if (m_personModel->itemTypeAt(index) == ItemType::Person)
            {
                QString id = m_personModel->personIdAt(index);
                if (!id.isEmpty() && !ids.contains(id))
                {
                    ids.append(id);
                }
            }
        }
    }

    return ids;
}

void WardListDialog::onMapFamilyClicked(const QString& familyId)
{
    if (m_mode == FamilyMode && m_familyModel)
    {
        // Direct selection
        QModelIndex index = m_familyModel->indexForFamilyId(familyId);
        if (index.isValid())
        {
            m_treeView->selectionModel()->select(
                index, QItemSelectionModel::ClearAndSelect);
            m_treeView->scrollTo(index);
        }
    }
    else if (m_personModel)
    {
        // In person mode, select the first person from this family
        std::optional<Family> familyOpt =
            m_documentManager->document().findFamilyById(familyId);
        if (familyOpt && !familyOpt->members().isEmpty())
        {
            QString personId = familyOpt->members().first().id();
            QModelIndex index = m_personModel->indexForPersonId(personId);
            if (index.isValid())
            {
                m_treeView->selectionModel()->select(
                    index, QItemSelectionModel::ClearAndSelect);
                m_treeView->scrollTo(index);
            }
        }
    }
}

void WardListDialog::onSelectionChanged()
{
    // Update map highlighting
    m_mapWidget->updateHighlights();
}

QString WardListDialog::familyIdForCurrentSelection() const
{
    QModelIndex current = m_treeView->currentIndex();
    if (!current.isValid())
    {
        return QString();
    }

    if (m_mode == FamilyMode && m_familyModel)
    {
        return m_familyModel->familyIdAt(current);
    }
    else if (m_personModel)
    {
        // Look up family for selected person
        QString personId = m_personModel->personIdAt(current);
        if (!personId.isEmpty())
        {
            return m_documentManager->document().familyIdForPerson(personId);
        }
    }
    return QString();
}

HighlightInfo WardListDialog::highlightInfo() const
{
    HighlightInfo info;
    QString familyId = familyIdForCurrentSelection();
    if (!familyId.isEmpty())
    {
        info.highlightedFamilyIds.insert(familyId);
    }
    return info;
}

QSet<QString> WardListDialog::visibleFamilyIds() const
{
    // Show all families in the dialog
    return {};
}

// Static convenience methods

QString WardListDialog::selectFamily(DocumentManager* documentManager,
                                     const QString& initialId,
                                     QWidget* parent)
{
    WardListDialog dialog(documentManager, FamilyMode, parent);
    dialog.setSelectionMode(SingleSelect);
    if (!initialId.isEmpty())
    {
        dialog.setPreselectedIds({initialId});
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        QStringList ids = dialog.selectedIds();
        return ids.isEmpty() ? QString() : ids.first();
    }
    return QString();
}

QStringList WardListDialog::selectFamilies(DocumentManager* documentManager,
                                           const QStringList& initialIds,
                                           QWidget* parent)
{
    WardListDialog dialog(documentManager, FamilyMode, parent);
    dialog.setSelectionMode(MultiSelect);
    if (!initialIds.isEmpty())
    {
        dialog.setPreselectedIds(initialIds);
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        return dialog.selectedIds();
    }
    return {};
}

QString WardListDialog::selectPerson(DocumentManager* documentManager,
                                     const QString& initialId,
                                     QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, parent);
    dialog.setSelectionMode(SingleSelect);
    if (!initialId.isEmpty())
    {
        dialog.setPreselectedIds({initialId});
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        QStringList ids = dialog.selectedIds();
        return ids.isEmpty() ? QString() : ids.first();
    }
    return QString();
}

QStringList WardListDialog::selectPersons(DocumentManager* documentManager,
                                          const QStringList& initialIds,
                                          QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, parent);
    dialog.setSelectionMode(MultiSelect);
    if (!initialIds.isEmpty())
    {
        dialog.setPreselectedIds(initialIds);
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        return dialog.selectedIds();
    }
    return {};
}
