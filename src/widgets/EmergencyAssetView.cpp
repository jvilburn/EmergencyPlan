#include "EmergencyAssetView.h"
#include "BaseTreeModel.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyAsset.h"
#include "EmergencyAssetModel.h"
#include "FilterBar.h"
#include "Person.h"
#include "Phone.h"
#include "EmergencyAssetCommands.h"
#include "SelectionPreservingTreeView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

EmergencyAssetView::EmergencyAssetView(DocumentManager* documentManager,
                                              ResponseArea area,
                                              QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // FilterBar owns Filter
    m_filterBar = new FilterBar(documentManager, this);
    layout->addWidget(m_filterBar);

    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);

    m_addButton = new QPushButton(tr("Add"));
    m_editButton = new QPushButton(tr("Edit"));
    m_deleteButton = new QPushButton(tr("Delete"));

    toolbar->addWidget(m_addButton);
    toolbar->addWidget(m_editButton);
    toolbar->addWidget(m_deleteButton);
    toolbar->addStretch();

    layout->addLayout(toolbar);

    // Create model with filter from FilterBar
    m_model = new EmergencyAssetModel(documentManager, m_filterBar->filter(), area, this);

    // Create tree view with model
    m_tree = new SelectionPreservingTreeView(m_model, this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    m_tree->expandToDepth(0);
    layout->addWidget(m_tree);

    // Connections
    connect(m_addButton, &QPushButton::clicked, this, &EmergencyAssetView::addAsset);
    connect(m_editButton, &QPushButton::clicked, this, &EmergencyAssetView::editAsset);
    connect(m_deleteButton, &QPushButton::clicked, this, &EmergencyAssetView::deleteAsset);

    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &EmergencyAssetView::onSelectionChanged);
    connect(m_tree, &QTreeView::doubleClicked,
            this, &EmergencyAssetView::onTreeDoubleClicked);
    connect(m_tree, &QTreeView::customContextMenuRequested,
            this, &EmergencyAssetView::onContextMenu);

    // Expand top-level items when model is reset
    connect(m_model, &QAbstractItemModel::modelReset, this, &EmergencyAssetView::expandAssets);

    // Load contact details when person node is expanded
    connect(m_tree, &QTreeView::expanded,
            this, &EmergencyAssetView::onTreeExpanded);

    updateButtonStates();
}

void EmergencyAssetView::onSelectionChanged()
{
    updateButtonStates();
    emit highlightChanged();
}

void EmergencyAssetView::onTreeDoubleClicked(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    switch (type)
    {
    case ItemType::Asset:
        editAsset();
        break;

    case ItemType::Person:
        // Could open person details in future
        break;

    case ItemType::ContactDetail:
        // Contact details are read-only display nodes
        break;

    case ItemType::Invalid:
        break;
    }
}

void EmergencyAssetView::onTreeExpanded(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    if (type == ItemType::Person)
    {
        m_model->loadContactDetails(index);
    }
}

void EmergencyAssetView::onContextMenu(const QPoint& pos)
{
    QModelIndex index = m_tree->indexAt(pos);

    QMenu menu;

    if (!index.isValid())
    {
        // No item - show add option
        menu.addAction(tr("Add Asset..."), this, &EmergencyAssetView::addAsset);
    }
    else
    {
        ItemType type = m_model->itemTypeAt(index);

        switch (type)
        {
        case ItemType::Asset:
            {
                m_contextAssetId = m_model->assetIdAt(index);
                menu.addAction(tr("Select People..."), this, &EmergencyAssetView::selectPeopleFromContextMenu);
                menu.addSeparator();
                menu.addAction(tr("Rename..."), this, &EmergencyAssetView::editAsset);
                menu.addAction(tr("Delete"), this, &EmergencyAssetView::deleteAsset);
            }
            break;

        case ItemType::Person:
            {
                // Show contact info (disabled) if available
                auto personIdOpt = m_model->personIdAt(index);
                if (!personIdOpt)
                {
                    break;
                }
                const Document& doc = m_documentManager->document();
                PersonId personId = *personIdOpt;
                std::optional<Person> personOpt = doc.findPersonById(personId);
                if (personOpt)
                {
                    const Phone& phone = personOpt->phone();
                    if (!phone.isEmpty())
                    {
                        QAction* phoneAction = menu.addAction(phone);
                        phoneAction->setEnabled(false);
                    }
                    const QString& email = personOpt->email();
                    if (!email.isEmpty())
                    {
                        QAction* emailAction = menu.addAction(email);
                        emailAction->setEnabled(false);
                    }
                    if (!phone.isEmpty() || !email.isEmpty())
                    {
                        menu.addSeparator();
                    }
                }

                m_contextAssetId = m_model->assetIdAt(index);
                m_contextPersonId = personId;
                menu.addAction(tr("Remove"), this, &EmergencyAssetView::removePersonFromContextMenu);
            }
            break;

        case ItemType::ContactDetail:
            // Contact details are read-only display nodes
            break;

        case ItemType::Invalid:
            break;
        }
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }

    // Clear context after menu closes (whether action taken or dismissed)
    m_contextAssetId = std::nullopt;
    m_contextPersonId = std::nullopt;
}

void EmergencyAssetView::expandAssets()
{
    m_tree->expandToDepth(0);
}

void EmergencyAssetView::selectPeopleFromContextMenu()
{
    auto assetId = m_contextAssetId;
    m_contextAssetId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (assetId)
    {
        showSelectPeopleDialog(*assetId);
    }
}

void EmergencyAssetView::removePersonFromContextMenu()
{
    auto assetId = m_contextAssetId;
    auto personId = m_contextPersonId;
    m_contextAssetId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (assetId && personId)
    {
        removePersonFromAsset(*assetId, *personId);
    }
}

void EmergencyAssetView::updateButtonStates()
{
    bool hasAssetSelected = selectedAssetId().has_value();
    m_editButton->setEnabled(hasAssetSelected);
    m_deleteButton->setEnabled(hasAssetSelected);
}

std::optional<EmergencyAssetId> EmergencyAssetView::selectedAssetId() const
{
    QModelIndex current = m_tree->currentIndex();
    if (!current.isValid())
    {
        return std::nullopt;
    }

    // For both Asset and Person items, assetIdAt returns the asset ID
    return m_model->assetIdAt(current);
}

HighlightInfo EmergencyAssetView::highlightInfo() const
{
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}

QSet<FamilyId> EmergencyAssetView::visibleFamilyIds() const
{
    // Show all families
    return {};
}

void EmergencyAssetView::clearSelection()
{
    m_tree->clearSelection();
}

void EmergencyAssetView::selectFamily(const FamilyId& familyId)
{
    QModelIndex idx = m_model->indexForFamilyId(familyId);
    if (idx.isValid())
    {
        m_tree->setCurrentIndex(idx);
        m_tree->scrollTo(idx);
    }
}

void EmergencyAssetView::addAsset()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Asset"),
                                          tr("Asset name:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty())
    {
        EmergencyAsset asset = EmergencyAsset::create(name, m_model->area());
        m_documentManager->executeCommand(
            std::make_unique<AddEmergencyAssetCommand>(asset));
    }
}

void EmergencyAssetView::editAsset()
{
    auto assetId = selectedAssetId();
    if (!assetId)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<EmergencyAsset> assetOpt = doc.findEmergencyAssetById(*assetId);
    if (!assetOpt)
    {
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, tr("Rename Asset"),
                                          tr("Asset name:"),
                                          QLineEdit::Normal, assetOpt->name(), &ok);
    if (ok && !name.isEmpty() && name != assetOpt->name())
    {
        EmergencyAsset updated = *assetOpt;
        updated.setName(name);
        m_documentManager->executeCommand(
            std::make_unique<UpdateEmergencyAssetCommand>(*assetOpt, updated));
    }
}

void EmergencyAssetView::deleteAsset()
{
    auto assetId = selectedAssetId();
    if (!assetId)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<EmergencyAsset> assetOpt = doc.findEmergencyAssetById(*assetId);
    if (!assetOpt)
    {
        return;
    }

    QString message = tr("Delete asset \"%1\"?").arg(assetOpt->name());
    if (QMessageBox::question(this, tr("Delete Asset"), message) == QMessageBox::Yes)
    {
        m_documentManager->executeCommand(
            std::make_unique<DeleteEmergencyAssetCommand>(*assetOpt));
    }
}

void EmergencyAssetView::showSelectPeopleDialog(const EmergencyAssetId& assetId)
{
    const Document& doc = m_documentManager->document();
    std::optional<EmergencyAsset> assetOpt = doc.findEmergencyAssetById(assetId);
    if (!assetOpt)
    {
        return;
    }

    QList<PersonId> currentIds = assetOpt->personIds().values();

    auto result = WardListDialog::selectPersons(
        m_documentManager, assetOpt->name(), currentIds, this);

    if (!result)
    {
        return;  // Cancelled
    }

    QSet<PersonId> newSet(result->begin(), result->end());
    if (newSet == assetOpt->personIds())
    {
        return;  // No change
    }

    EmergencyAsset updated = *assetOpt;
    updated.setPersonIds(newSet);
    m_documentManager->executeCommand(
        std::make_unique<UpdateEmergencyAssetCommand>(*assetOpt, updated));
}

void EmergencyAssetView::removePersonFromAsset(const EmergencyAssetId& assetId, const PersonId& personId)
{
    m_documentManager->executeCommand(
        std::make_unique<UnassignEmergencyAssetFromPersonCommand>(assetId, personId));
}
