#include "EmergencyResourceView.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyResource.h"
#include "EmergencyResourceModel.h"
#include "Person.h"
#include "Phone.h"
#include "EmergencyResourceCommands.h"

#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

EmergencyResourceView::EmergencyResourceView(DocumentManager* documentManager, ResponseArea area, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_area(area)
    , m_model(nullptr)
    , m_tree(nullptr)
    , m_addButton(nullptr)
    , m_editButton(nullptr)
    , m_deleteButton(nullptr)
    , m_contextResourceId()
    , m_contextPersonId()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

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

    // Create model
    m_model = new EmergencyResourceModel(m_documentManager, m_area, this);

    // Create tree view
    m_tree = new QTreeView();
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    m_tree->setModel(m_model);
    m_tree->expandToDepth(0);  // Expand only top-level (resources)
    layout->addWidget(m_tree);

    // Connections
    connect(m_addButton, &QPushButton::clicked, this, &EmergencyResourceView::addResource);
    connect(m_editButton, &QPushButton::clicked, this, &EmergencyResourceView::editResource);
    connect(m_deleteButton, &QPushButton::clicked, this, &EmergencyResourceView::deleteResource);

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &EmergencyResourceView::onSelectionChanged);
    connect(m_tree, &QTreeView::doubleClicked,
            this, &EmergencyResourceView::onTreeDoubleClicked);
    connect(m_tree, &QTreeView::customContextMenuRequested,
            this, &EmergencyResourceView::onContextMenu);

    // Expand top-level items when model is reset
    connect(m_model, &QAbstractItemModel::modelReset, this, &EmergencyResourceView::expandResources);

    // Load contact details when person node is expanded
    connect(m_tree, &QTreeView::expanded,
            this, &EmergencyResourceView::onTreeExpanded);

    updateButtonStates();
}

void EmergencyResourceView::onSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(current)
    Q_UNUSED(previous)
    updateButtonStates();
    emit highlightChanged();
}

void EmergencyResourceView::onTreeDoubleClicked(const QModelIndex& index)
{
    EmergencyResourceModel::ItemType type = m_model->itemTypeAt(index);

    switch (type)
    {
    case EmergencyResourceModel::ItemType::Resource:
        editResource();
        break;

    case EmergencyResourceModel::ItemType::Person:
        // Could open person details in future
        break;

    case EmergencyResourceModel::ItemType::ContactDetail:
        // Contact details are read-only display nodes
        break;

    case EmergencyResourceModel::ItemType::Invalid:
        break;
    }
}

void EmergencyResourceView::onTreeExpanded(const QModelIndex& index)
{
    EmergencyResourceModel::ItemType type = m_model->itemTypeAt(index);

    if (type == EmergencyResourceModel::ItemType::Person)
    {
        m_model->loadContactDetails(index);
    }
}

void EmergencyResourceView::onContextMenu(const QPoint& pos)
{
    QModelIndex index = m_tree->indexAt(pos);

    QMenu menu;

    if (!index.isValid())
    {
        // No item - show add option
        menu.addAction(tr("Add Resource..."), this, &EmergencyResourceView::addResource);
    }
    else
    {
        EmergencyResourceModel::ItemType type = m_model->itemTypeAt(index);
        QString id = m_model->idAt(index);

        switch (type)
        {
        case EmergencyResourceModel::ItemType::Resource:
            {
                m_contextResourceId = id;
                menu.addAction(tr("Select People..."), this, &EmergencyResourceView::selectPeopleFromContextMenu);
                menu.addSeparator();
                menu.addAction(tr("Rename..."), this, &EmergencyResourceView::editResource);
                menu.addAction(tr("Delete"), this, &EmergencyResourceView::deleteResource);
            }
            break;

        case EmergencyResourceModel::ItemType::Person:
            {
                // Show contact info (disabled) if available
                const Document& doc = m_documentManager->document();
                std::optional<Person> personOpt = doc.findPersonById(id);
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

                m_contextResourceId = m_model->resourceIdAt(index);
                m_contextPersonId = id;
                menu.addAction(tr("Remove"), this, &EmergencyResourceView::removePersonFromContextMenu);
            }
            break;

        case EmergencyResourceModel::ItemType::ContactDetail:
            // Contact details are read-only display nodes
            break;

        case EmergencyResourceModel::ItemType::Invalid:
            break;
        }
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }

    // Clear context after menu closes (whether action taken or dismissed)
    m_contextResourceId.clear();
    m_contextPersonId.clear();
}

void EmergencyResourceView::expandResources()
{
    m_tree->expandToDepth(0);
}

void EmergencyResourceView::selectPeopleFromContextMenu()
{
    QString resourceId = m_contextResourceId;
    m_contextResourceId.clear();
    m_contextPersonId.clear();
    showSelectPeopleDialog(resourceId);
}

void EmergencyResourceView::removePersonFromContextMenu()
{
    QString resourceId = m_contextResourceId;
    QString personId = m_contextPersonId;
    m_contextResourceId.clear();
    m_contextPersonId.clear();
    removePersonFromResource(resourceId, personId);
}

void EmergencyResourceView::updateButtonStates()
{
    QString resourceId = selectedResourceId();
    bool hasResourceSelected = !resourceId.isEmpty();
    m_editButton->setEnabled(hasResourceSelected);
    m_deleteButton->setEnabled(hasResourceSelected);
}

QString EmergencyResourceView::selectedResourceId() const
{
    QModelIndex current = m_tree->currentIndex();
    if (!current.isValid())
    {
        return QString();
    }

    // For both Resource and Person items, resourceIdAt returns the resource ID
    return m_model->resourceIdAt(current);
}

HighlightInfo EmergencyResourceView::highlightInfo() const
{
    HighlightInfo info;

    QModelIndex current = m_tree->currentIndex();
    if (!current.isValid())
    {
        return info;
    }

    const Document& doc = m_documentManager->document();
    EmergencyResourceModel::ItemType type = m_model->itemTypeAt(current);
    QString id = m_model->idAt(current);

    switch (type)
    {
    case EmergencyResourceModel::ItemType::Person:
        {
            // Single person selected - highlight their family
            QString familyId = doc.familyIdForPerson(id);
            if (!familyId.isEmpty())
            {
                info.highlightedFamilyIds.insert(familyId);
            }
        }
        break;

    case EmergencyResourceModel::ItemType::Resource:
        {
            // Resource selected - highlight all families with people having this resource
            std::optional<EmergencyResource> resourceOpt = doc.findEmergencyResourceById(id);
            if (resourceOpt)
            {
                for (const QString& personId : resourceOpt->personIds())
                {
                    QString familyId = doc.familyIdForPerson(personId);
                    if (!familyId.isEmpty())
                    {
                        info.highlightedFamilyIds.insert(familyId);
                    }
                }
            }
        }
        break;

    case EmergencyResourceModel::ItemType::ContactDetail:
        {
            // Contact detail selected - highlight parent person's family
            QString personId = m_model->idAt(current);
            QString familyId = doc.familyIdForPerson(personId);
            if (!familyId.isEmpty())
            {
                info.highlightedFamilyIds.insert(familyId);
            }
        }
        break;

    case EmergencyResourceModel::ItemType::Invalid:
        break;
    }

    return info;
}

QSet<QString> EmergencyResourceView::visibleFamilyIds() const
{
    // Show all families
    return {};
}

void EmergencyResourceView::addResource()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Resource"),
                                          tr("Resource name:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty())
    {
        EmergencyResource resource = EmergencyResource::create(name, m_area);
        m_documentManager->executeCommand(
            std::make_unique<AddEmergencyResourceCommand>(resource));
    }
}

void EmergencyResourceView::editResource()
{
    QString resourceId = selectedResourceId();
    if (resourceId.isEmpty())
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<EmergencyResource> resourceOpt = doc.findEmergencyResourceById(resourceId);
    if (!resourceOpt)
    {
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, tr("Rename Resource"),
                                          tr("Resource name:"),
                                          QLineEdit::Normal, resourceOpt->name(), &ok);
    if (ok && !name.isEmpty() && name != resourceOpt->name())
    {
        EmergencyResource updated = *resourceOpt;
        updated.setName(name);
        m_documentManager->executeCommand(
            std::make_unique<UpdateEmergencyResourceCommand>(*resourceOpt, updated));
    }
}

void EmergencyResourceView::deleteResource()
{
    QString resourceId = selectedResourceId();
    if (resourceId.isEmpty())
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<EmergencyResource> resourceOpt = doc.findEmergencyResourceById(resourceId);
    if (!resourceOpt)
    {
        return;
    }

    QString message = tr("Delete resource \"%1\"?").arg(resourceOpt->name());
    if (QMessageBox::question(this, tr("Delete Resource"), message) == QMessageBox::Yes)
    {
        m_documentManager->executeCommand(
            std::make_unique<DeleteEmergencyResourceCommand>(*resourceOpt));
    }
}

void EmergencyResourceView::showSelectPeopleDialog(const QString& resourceId)
{
    const Document& doc = m_documentManager->document();
    std::optional<EmergencyResource> resourceOpt = doc.findEmergencyResourceById(resourceId);
    if (!resourceOpt)
    {
        return;
    }

    // Get current person IDs as a list
    QStringList currentIds = resourceOpt->personIds().values();

    // Show dialog to select persons
    QStringList selectedIds = WardListDialog::selectPersons(m_documentManager, currentIds, this);

    // Check if selection changed
    QSet<QString> newSet(selectedIds.begin(), selectedIds.end());
    if (newSet == resourceOpt->personIds())
    {
        return;  // No change
    }

    // Update resource with new person IDs (single undo operation)
    EmergencyResource updated = *resourceOpt;
    updated.setPersonIds(newSet);
    m_documentManager->executeCommand(
        std::make_unique<UpdateEmergencyResourceCommand>(*resourceOpt, updated));
}

void EmergencyResourceView::removePersonFromResource(const QString& resourceId, const QString& personId)
{
    m_documentManager->executeCommand(
        std::make_unique<UnassignEmergencyResourceFromPersonCommand>(resourceId, personId));
}
