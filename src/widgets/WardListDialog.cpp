#include "WardListDialog.h"
#include "FilterBar.h"
#include "FamilyTreeModel.h"
#include "PersonTreeModel.h"
#include "ItemType.h"
#include "MapWidget.h"
#include "DocumentManager.h"
#include "Document.h"
#include "SelectionPreservingTreeView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QSet>

WardListDialog::WardListDialog(DocumentManager* documentManager,
                               Mode mode,
                               bool checkable,
                               QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_documentManager(documentManager)
    , m_checkable(checkable)
{
    setupUi();
}

QString WardListDialog::name() const
{
    return m_nameEdit->text().trimmed();
}

void WardListDialog::setupUi()
{
    resize(1000, 700);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Name field row
    QHBoxLayout* nameLayout = new QHBoxLayout();
    m_nameLabel = new QLabel(this);
    m_nameEdit = new QLineEdit(this);
    nameLayout->addWidget(m_nameLabel);
    nameLayout->addWidget(m_nameEdit, 1);
    mainLayout->addLayout(nameLayout);

    // Create FilterBar (owns Filter internally)
    m_filterBar = new FilterBar(m_documentManager, this);
    mainLayout->addWidget(m_filterBar);

    // Create splitter with tree and map
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Create appropriate model based on mode, then tree view with model
    if (m_mode == FamilyMode)
    {
        m_familyModel = new FamilyTreeModel(m_documentManager, m_filterBar->filter(), m_checkable, this);
        m_treeView = new SelectionPreservingTreeView(m_familyModel, this);
    }
    else
    {
        m_personModel = new PersonTreeModel(m_documentManager, m_filterBar->filter(), m_checkable, this);
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

QSet<FamilyId> WardListDialog::highlightedFamilyIds() const
{
    if (m_checkable)
    {
        // In checkbox mode, highlight all checked items' families
        QSet<FamilyId> familyIds;
        if (m_mode == FamilyMode && m_familyModel)
        {
            familyIds = m_familyModel->checkedFamilyIds();
        }
        else if (m_personModel)
        {
            const Document& doc = m_documentManager->document();
            for (const PersonId& personId : m_personModel->checkedPersonIds())
            {
                auto familyId = doc.familyIdForPerson(personId);
                if (familyId)
                {
                    familyIds.insert(*familyId);
                }
            }
        }
        return familyIds;
    }

    // Single-select: highlight current selection
    QModelIndex current = m_treeView->currentIndex();
    if (!current.isValid())
    {
        return {};
    }

    if (m_mode == FamilyMode && m_familyModel)
    {
        auto id = m_familyModel->familyIdAt(current);
        if (id)
        {
            return {*id};
        }
    }
    else if (m_personModel)
    {
        auto personId = m_personModel->personIdAt(current);
        if (personId)
        {
            auto familyId = m_documentManager->document().familyIdForPerson(*personId);
            if (familyId)
            {
                return {*familyId};
            }
        }
    }
    return {};
}

HighlightInfo WardListDialog::highlightInfo() const
{
    HighlightInfo info;
    info.highlightedFamilyIds = highlightedFamilyIds();
    return info;
}

QSet<FamilyId> WardListDialog::visibleFamilyIds() const
{
    // Show all families in the dialog
    return {};
}

// Static convenience methods

std::optional<FamilySelectionResult> WardListDialog::selectFamilies(
    DocumentManager* documentManager,
    const QString& nameLabel,
    const QString& initialName,
    const QList<FamilyId>& initialIds,
    QWidget* parent)
{
    WardListDialog dialog(documentManager, FamilyMode, true, parent);
    dialog.setWindowTitle(tr("Select Families \u2014 %1").arg(nameLabel));
    dialog.m_nameLabel->setText(nameLabel + tr(":"));
    dialog.m_nameEdit->setText(initialName);
    connect(dialog.m_familyModel, &FamilyTreeModel::dataChanged,
            dialog.m_mapWidget, &MapWidget::updateHighlights);
    if (!initialIds.isEmpty())
    {
        QSet<FamilyId> idSet(initialIds.begin(), initialIds.end());
        dialog.m_familyModel->setCheckedFamilyIds(idSet);

        // Scroll to first checked family
        QModelIndex firstIndex = dialog.m_familyModel->indexForFamilyId(initialIds.first());
        if (firstIndex.isValid())
        {
            dialog.m_treeView->scrollTo(firstIndex);
        }
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    QSet<FamilyId> checked = dialog.m_familyModel->checkedFamilyIds();
    FamilySelectionResult result;
    result.name = dialog.name();
    result.familyIds = QList<FamilyId>(checked.begin(), checked.end());
    return result;
}

std::optional<PersonSelectionResult> WardListDialog::selectPersonWithName(
    DocumentManager* documentManager,
    const QString& nameLabel,
    const QString& initialName,
    const std::optional<PersonId>& initialId,
    QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, false, parent);
    dialog.setWindowTitle(tr("Select Person \u2014 %1").arg(nameLabel));
    dialog.m_nameLabel->setText(nameLabel + tr(":"));
    dialog.m_nameEdit->setText(initialName);
    if (initialId)
    {
        dialog.setPreselectedPersonIds({*initialId});
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    PersonSelectionResult result;
    result.name = dialog.name();
    // Single-select: get from current selection
    result.personIds = dialog.selectedPersonIds();
    return result;
}

std::optional<PersonSelectionResult> WardListDialog::selectPersons(
    DocumentManager* documentManager,
    const QString& nameLabel,
    const QString& initialName,
    const QList<PersonId>& initialIds,
    QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, true, parent);
    dialog.setWindowTitle(tr("Select People \u2014 %1").arg(nameLabel));
    dialog.m_nameLabel->setText(nameLabel + tr(":"));
    dialog.m_nameEdit->setText(initialName);
    connect(dialog.m_personModel, &PersonTreeModel::dataChanged,
            dialog.m_mapWidget, &MapWidget::updateHighlights);
    if (!initialIds.isEmpty())
    {
        QSet<PersonId> idSet(initialIds.begin(), initialIds.end());
        dialog.m_personModel->setCheckedPersonIds(idSet);

        // Scroll to first checked person
        QModelIndex firstIndex = dialog.m_personModel->indexForPersonId(initialIds.first());
        if (firstIndex.isValid())
        {
            dialog.m_treeView->scrollTo(firstIndex);
        }
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    QSet<PersonId> checked = dialog.m_personModel->checkedPersonIds();
    PersonSelectionResult result;
    result.name = dialog.name();
    result.personIds = QList<PersonId>(checked.begin(), checked.end());
    return result;
}
