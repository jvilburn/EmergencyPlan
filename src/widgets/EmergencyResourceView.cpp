#include "EmergencyResourceView.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "DocumentChange.h"
#include "EmergencyResource.h"
#include "Person.h"
#include "Phone.h"
#include "Family.h"
#include "EmergencyResourceCommands.h"

#include <QTreeWidget>
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
    , m_tree(nullptr)
    , m_addButton(nullptr)
    , m_editButton(nullptr)
    , m_deleteButton(nullptr)
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

    // Tree
    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    layout->addWidget(m_tree);

    // Connections
    connect(m_addButton, &QPushButton::clicked, this, &EmergencyResourceView::addResource);
    connect(m_editButton, &QPushButton::clicked, this, &EmergencyResourceView::editResource);
    connect(m_deleteButton, &QPushButton::clicked, this, &EmergencyResourceView::deleteResource);

    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &EmergencyResourceView::onTreeItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &EmergencyResourceView::onContextMenu);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &EmergencyResourceView::onSelectionChanged);
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &EmergencyResourceView::onDocumentChanged);

    rebuildTree();
    updateButtonStates();
}

void EmergencyResourceView::rebuildTree()
{
    m_tree->clear();

    const Document& doc = m_documentManager->document();
    QList<EmergencyResource> resources = doc.emergencyResourcesByArea(m_area);

    // Sort by name
    std::sort(resources.begin(), resources.end(),
              [](const EmergencyResource& a, const EmergencyResource& b)
              {
                  return a.name().toLower() < b.name().toLower();
              });

    for (const EmergencyResource& resource : resources)
    {
        QTreeWidgetItem* resourceItem = new QTreeWidgetItem();
        resourceItem->setText(0, QString("%1 (%2)").arg(resource.name()).arg(resource.personIds().size()));
        resourceItem->setData(0, IdRole, resource.id());
        resourceItem->setData(0, TypeRole, static_cast<int>(ItemType::EmergencyResource));

        // Bold if selected
        if (m_selectedResourceId == resource.id())
        {
            QFont font = resourceItem->font(0);
            font.setBold(true);
            resourceItem->setFont(0, font);
        }

        // Get persons assigned to this resource, sorted by display name
        QList<QPair<QString, QString>> persons;  // (personId, displayName)
        for (const QString& personId : resource.personIds())
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (person)
            {
                persons.append({personId, person->displayName()});
            }
        }
        std::sort(persons.begin(), persons.end(),
                  [](const auto& a, const auto& b)
                  {
                      return a.second.toLower() < b.second.toLower();
                  });

        // Add person items
        for (const auto& [personId, displayName] : persons)
        {
            QTreeWidgetItem* personItem = new QTreeWidgetItem(resourceItem);
            personItem->setText(0, displayName);
            personItem->setData(0, IdRole, personId);
            personItem->setData(0, TypeRole, static_cast<int>(ItemType::Person));
            personItem->setData(0, SecondaryIdRole, resource.id());

            // Bold if selected
            if (m_selectedPersonId == personId)
            {
                QFont font = personItem->font(0);
                font.setBold(true);
                personItem->setFont(0, font);
            }
        }

        m_tree->addTopLevelItem(resourceItem);
        resourceItem->setExpanded(true);
    }
}

void EmergencyResourceView::updateButtonStates()
{
    bool hasResourceSelected = !m_selectedResourceId.isEmpty();
    m_editButton->setEnabled(hasResourceSelected);
    m_deleteButton->setEnabled(hasResourceSelected);
}

void EmergencyResourceView::validateSelections()
{
    const Document& doc = m_documentManager->document();

    // Clear resource selection if it no longer exists
    if (!m_selectedResourceId.isEmpty()
        && !doc.findEmergencyResourceById(m_selectedResourceId))
    {
        m_selectedResourceId.clear();
    }

    // Clear person selection if they are no longer in the resource
    if (!m_selectedPersonId.isEmpty())
    {
        bool valid = false;
        if (!m_selectedResourceId.isEmpty())
        {
            std::optional<EmergencyResource> resourceOpt = doc.findEmergencyResourceById(m_selectedResourceId);
            if (resourceOpt && resourceOpt->personIds().contains(m_selectedPersonId))
            {
                valid = true;
            }
        }
        if (!valid)
        {
            m_selectedPersonId.clear();
        }
    }
}

void EmergencyResourceView::onDocumentChanged(const DocumentChange& change)
{
    switch (change.scope)
    {
    case ChangeScope::Full:
    case ChangeScope::EmergencyResource:
    case ChangeScope::Family:  // Person names are in Family
        validateSelections();
        rebuildTree();
        updateButtonStates();
        emit highlightChanged();
        break;
    default:
        break;
    }
}

void EmergencyResourceView::onTreeItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    switch (type)
    {
    case ItemType::EmergencyResource:
        editResource();
        break;

    case ItemType::Person:
        // Could open person details in future
        break;
    }
}

void EmergencyResourceView::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);

    QMenu menu;

    if (!item)
    {
        // No item - show add option
        menu.addAction(tr("Add Resource..."), this, &EmergencyResourceView::addResource);
    }
    else
    {
        ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
        QString id = item->data(0, IdRole).toString();

        switch (type)
        {
        case ItemType::EmergencyResource:
            {
                menu.addAction(tr("Select People..."), this, [this, id]()
                {
                    showSelectPeopleDialog(id);
                });
                menu.addSeparator();
                menu.addAction(tr("Rename..."), this, &EmergencyResourceView::editResource);
                menu.addAction(tr("Delete"), this, &EmergencyResourceView::deleteResource);
            }
            break;

        case ItemType::Person:
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

                QString resourceId = item->data(0, SecondaryIdRole).toString();
                menu.addAction(tr("Remove"), this, [this, resourceId, id]()
                {
                    removePersonFromResource(resourceId, id);
                });
            }
            break;
        }
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }
}

void EmergencyResourceView::onSelectionChanged()
{
    // Un-bold all items
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* resourceItem = m_tree->topLevelItem(i);
        QFont resourceFont = resourceItem->font(0);
        resourceFont.setBold(false);
        resourceItem->setFont(0, resourceFont);

        for (int j = 0; j < resourceItem->childCount(); ++j)
        {
            QTreeWidgetItem* personItem = resourceItem->child(j);
            QFont personFont = personItem->font(0);
            personFont.setBold(false);
            personItem->setFont(0, personFont);
        }
    }

    // Sync selection state from tree
    QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    m_selectedResourceId.clear();
    m_selectedPersonId.clear();

    if (!selected.isEmpty())
    {
        QTreeWidgetItem* item = selected.first();
        ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
        QString id = item->data(0, IdRole).toString();

        switch (type)
        {
        case ItemType::EmergencyResource:
            m_selectedResourceId = id;
            break;

        case ItemType::Person:
            m_selectedPersonId = id;
            m_selectedResourceId = item->data(0, SecondaryIdRole).toString();
            break;
        }

        // Bold the selected item
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
    }

    updateButtonStates();
    emit highlightChanged();
}

HighlightInfo EmergencyResourceView::highlightInfo() const
{
    HighlightInfo info;
    const Document& doc = m_documentManager->document();

    if (!m_selectedPersonId.isEmpty())
    {
        // Single person selected - highlight their family
        QString familyId = doc.familyIdForPerson(m_selectedPersonId);
        if (!familyId.isEmpty())
        {
            info.highlightedFamilyIds.insert(familyId);
        }
    }
    else if (!m_selectedResourceId.isEmpty())
    {
        // Resource selected - highlight all families with people having this resource
        std::optional<EmergencyResource> resourceOpt = doc.findEmergencyResourceById(m_selectedResourceId);
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
    if (m_selectedResourceId.isEmpty())
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<EmergencyResource> resourceOpt = doc.findEmergencyResourceById(m_selectedResourceId);
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
    if (m_selectedResourceId.isEmpty())
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    std::optional<EmergencyResource> resourceOpt = doc.findEmergencyResourceById(m_selectedResourceId);
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
