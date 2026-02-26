#include "WardListDialog.h"
#include "FilterBar.h"
#include "FamilyTreeModel.h"
#include "PersonTreeModel.h"
#include "ItemType.h"
#include "MapWidget.h"
#include "DocumentManager.h"
#include "Document.h"
#include "SelectionPreservingTreeView.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QSet>

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

    // Create appropriate model based on mode, then tree view with model
    if (m_mode == FamilyMode)
    {
        m_familyModel = new FamilyTreeModel(m_documentManager, m_filterBar->filter(), this);
        m_treeView = new SelectionPreservingTreeView(m_familyModel, this);
    }
    else
    {
        m_personModel = new PersonTreeModel(m_documentManager, m_filterBar->filter(), this);
        m_treeView = new SelectionPreservingTreeView(m_personModel, this);
    }
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setIndentation(16);

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
    connect(m_treeView, &SelectionPreservingTreeView::selectionChanged,
            this, &WardListDialog::onSelectionChanged);
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

void WardListDialog::setPreselectedFamilyIds(const QList<FamilyId>& ids)
{
    QItemSelectionModel* selModel = m_treeView->selectionModel();
    selModel->clearSelection();

    bool scrolledToFirst = false;
    for (const FamilyId& id : ids)
    {
        QModelIndex index = m_familyModel->indexForFamilyId(id);
        if (index.isValid())
        {
            selModel->select(index, QItemSelectionModel::Select);
            if (!scrolledToFirst)
            {
                m_treeView->scrollTo(index);
                scrolledToFirst = true;
            }
        }
    }
}

void WardListDialog::setPreselectedPersonIds(const QList<PersonId>& ids)
{
    QItemSelectionModel* selModel = m_treeView->selectionModel();
    selModel->clearSelection();

    bool scrolledToFirst = false;
    for (const PersonId& id : ids)
    {
        QModelIndex index = m_personModel->indexForPersonId(id);
        if (index.isValid())
        {
            selModel->select(index, QItemSelectionModel::Select);
            if (!scrolledToFirst)
            {
                m_treeView->scrollTo(index);
                scrolledToFirst = true;
            }
        }
    }
}

QList<FamilyId> WardListDialog::selectedFamilyIds() const
{
    QList<FamilyId> ids;
    QSet<FamilyId> seen;
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();

    for (const QModelIndex& index : selected)
    {
        if (m_familyModel && m_familyModel->itemTypeAt(index) == ItemType::Family)
        {
            auto id = m_familyModel->familyIdAt(index);
            if (id && !seen.contains(*id))
            {
                seen.insert(*id);
                ids.append(*id);
            }
        }
    }

    return ids;
}

QList<PersonId> WardListDialog::selectedPersonIds() const
{
    QList<PersonId> ids;
    QSet<PersonId> seen;
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();

    for (const QModelIndex& index : selected)
    {
        if (m_personModel && m_personModel->itemTypeAt(index) == ItemType::Person)
        {
            auto id = m_personModel->personIdAt(index);
            if (id && !seen.contains(*id))
            {
                seen.insert(*id);
                ids.append(*id);
            }
        }
    }

    return ids;
}

void WardListDialog::selectFamily(const FamilyId& familyId)
{
    if (m_mode == FamilyMode && m_familyModel)
    {
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
            PersonId personId = familyOpt->members().first().id();
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

std::optional<FamilyId> WardListDialog::familyIdForCurrentSelection() const
{
    QModelIndex current = m_treeView->currentIndex();
    if (!current.isValid())
    {
        return std::nullopt;
    }

    if (m_mode == FamilyMode && m_familyModel)
    {
        return m_familyModel->familyIdAt(current);
    }
    else if (m_personModel)
    {
        // Look up family for selected person
        auto personId = m_personModel->personIdAt(current);
        if (personId)
        {
            return m_documentManager->document().familyIdForPerson(*personId);
        }
    }
    return std::nullopt;
}

HighlightInfo WardListDialog::highlightInfo() const
{
    HighlightInfo info;
    auto familyId = familyIdForCurrentSelection();
    if (familyId)
    {
        info.highlightedFamilyIds.insert(*familyId);
    }
    return info;
}

QSet<FamilyId> WardListDialog::visibleFamilyIds() const
{
    // Show all families in the dialog
    return {};
}

// Static convenience methods

std::optional<FamilyId> WardListDialog::selectFamily(DocumentManager* documentManager,
                                     const std::optional<FamilyId>& initialId,
                                     QWidget* parent)
{
    WardListDialog dialog(documentManager, FamilyMode, parent);
    dialog.setSelectionMode(SingleSelect);
    if (initialId)
    {
        dialog.setPreselectedFamilyIds({*initialId});
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        QList<FamilyId> ids = dialog.selectedFamilyIds();
        return ids.isEmpty() ? std::nullopt : std::optional<FamilyId>(ids.first());
    }
    return std::nullopt;
}

QList<FamilyId> WardListDialog::selectFamilies(DocumentManager* documentManager,
                                           const QList<FamilyId>& initialIds,
                                           QWidget* parent)
{
    WardListDialog dialog(documentManager, FamilyMode, parent);
    dialog.setSelectionMode(MultiSelect);
    if (!initialIds.isEmpty())
    {
        dialog.setPreselectedFamilyIds(initialIds);
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        return dialog.selectedFamilyIds();
    }
    return {};
}

std::optional<PersonId> WardListDialog::selectPerson(DocumentManager* documentManager,
                                     const std::optional<PersonId>& initialId,
                                     QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, parent);
    dialog.setSelectionMode(SingleSelect);
    if (initialId)
    {
        dialog.setPreselectedPersonIds({*initialId});
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        QList<PersonId> ids = dialog.selectedPersonIds();
        return ids.isEmpty() ? std::nullopt : std::optional<PersonId>(ids.first());
    }
    return std::nullopt;
}

QList<PersonId> WardListDialog::selectPersons(DocumentManager* documentManager,
                                          const QList<PersonId>& initialIds,
                                          QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, parent);
    dialog.setSelectionMode(MultiSelect);
    if (!initialIds.isEmpty())
    {
        dialog.setPreselectedPersonIds(initialIds);
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        return dialog.selectedPersonIds();
    }
    return {};
}
