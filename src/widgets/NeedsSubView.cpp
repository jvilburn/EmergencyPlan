#include "NeedsSubView.h"
#include "BaseTreeModel.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "FilterBar.h"
#include "Person.h"
#include "Phone.h"
#include "Family.h"
#include "FamilyCommands.h"
#include "ItemType.h"
#include "NeedsModel.h"
#include "SelectionPreservingTreeView.h"

#include <QVBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>

namespace
{

/// Helper to update a person within their family
void updatePersonInFamily(DocumentManager* docMgr, const FamilyId& familyId, const Person& updatedPerson)
{
    const Document& doc = docMgr->document();
    std::optional<Family> familyOpt = doc.findFamilyById(familyId);
    if (!familyOpt)
    {
        return;
    }

    Family oldFamily = *familyOpt;
    Family newFamily = oldFamily;

    QList<Person> members = newFamily.members();
    for (int i = 0; i < members.size(); ++i)
    {
        if (members[i].id() == updatedPerson.id())
        {
            members[i] = updatedPerson;
            break;
        }
    }
    newFamily.setMembers(members);

    docMgr->executeCommand(
        std::make_unique<UpdateFamilyCommand>(oldFamily, newFamily));
}

}  // namespace

NeedsSubView::NeedsSubView(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // FilterBar owns Filter
    m_filterBar = new FilterBar(this);
    layout->addWidget(m_filterBar);

    // Add button
    QPushButton* addButton = new QPushButton(tr("Add Special Need..."));
    connect(addButton, &QPushButton::clicked, this, &NeedsSubView::addNeed);
    layout->addWidget(addButton);

    // Create model with filter from FilterBar
    m_model = new NeedsModel(m_filterBar->filter(), this);

    // Create tree view with model
    m_tree = new SelectionPreservingTreeView(m_model, this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    layout->addWidget(m_tree);

    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &NeedsSubView::onSelectionChanged);
    connect(m_tree, &QTreeView::customContextMenuRequested,
            this, &NeedsSubView::onContextMenu);
    connect(m_tree, &QTreeView::expanded,
            this, &NeedsSubView::onTreeExpanded);
}

void NeedsSubView::onSelectionChanged()
{
    emit highlightChanged();
}

void NeedsSubView::onTreeExpanded(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    if (type == ItemType::Person)
    {
        m_model->loadContactDetails(index);
    }
}

void NeedsSubView::onContextMenu(const QPoint& pos)
{
    QModelIndex index = m_tree->indexAt(pos);
    if (!index.isValid())
    {
        return;  // No context menu on empty space (use the Add button instead)
    }

    auto personId = m_model->personIdAt(index);
    auto familyId = m_model->familyIdAt(index);
    if (!personId || !familyId)
    {
        return;
    }

    QMenu menu;

    const Document& doc = DocumentManager::instance()->document();

    // Show contact info (disabled) if available
    std::optional<Person> person = doc.findPersonById(*personId);
    if (person)
    {
        const Phone& phone = person->phone();
        if (!phone.isEmpty())
        {
            QAction* phoneAction = menu.addAction(phone);
            phoneAction->setEnabled(false);
        }
        const QString& email = person->email();
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

    m_contextPersonId = *personId;
    m_contextFamilyId = *familyId;
    menu.addAction(tr("Edit..."), this, &NeedsSubView::editNeedFromContextMenu);
    menu.addAction(tr("Delete"), this, &NeedsSubView::deleteNeedFromContextMenu);

    menu.exec(m_tree->viewport()->mapToGlobal(pos));

    m_contextPersonId = std::nullopt;
    m_contextFamilyId = std::nullopt;
}

HighlightInfo NeedsSubView::highlightInfo() const
{
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}

QSet<FamilyId> NeedsSubView::visibleFamilyIds() const
{
    // Show all families
    return {};
}

void NeedsSubView::clearSelection()
{
    m_tree->clearSelection();
}

void NeedsSubView::selectFamily(const FamilyId& familyId)
{
    QModelIndex idx = m_model->indexForFamilyId(familyId);
    if (idx.isValid())
    {
        m_tree->setCurrentIndex(idx);
        m_tree->scrollTo(idx);
    }
}

void NeedsSubView::addNeed()
{
    showNeedDialog(std::nullopt);
}

void NeedsSubView::editNeedFromContextMenu()
{
    std::optional<PersonId> personId = m_contextPersonId;
    m_contextPersonId = std::nullopt;
    m_contextFamilyId = std::nullopt;
    if (personId)
    {
        showNeedDialog(personId);
    }
}

void NeedsSubView::deleteNeedFromContextMenu()
{
    std::optional<PersonId> personId = m_contextPersonId;
    std::optional<FamilyId> familyId = m_contextFamilyId;
    m_contextPersonId = std::nullopt;
    m_contextFamilyId = std::nullopt;
    if (personId && familyId)
    {
        deleteNeed(*personId, *familyId);
    }
}

void NeedsSubView::showNeedDialog(const std::optional<PersonId>& personId)
{
    QString initialName;
    std::optional<PersonId> initialPersonId = personId;

    if (personId)
    {
        const Document& doc = DocumentManager::instance()->document();
        std::optional<Person> personOpt = doc.findPersonById(*personId);
        if (personOpt)
        {
            initialName = personOpt->specialNeedNote();
        }
    }

    std::optional<PersonSelectionResult> result = WardListDialog::selectPersonWithName(
        tr("Need"), initialName, initialPersonId, this);

    // Note: empty name is intentional for needs — it clears the special need note
    if (!result || result->personIds.isEmpty())
    {
        return;
    }

    PersonId selectedPersonId = result->personIds.first();
    const Document& doc = DocumentManager::instance()->document();
    std::optional<FamilyId> familyId = doc.familyIdForPerson(selectedPersonId);
    std::optional<Person> personOpt = doc.findPersonById(selectedPersonId);
    if (personOpt && familyId)
    {
        Person updatedPerson = *personOpt;
        updatedPerson.setSpecialNeedNote(result->name);
        updatePersonInFamily(DocumentManager::instance(), *familyId, updatedPerson);
    }
}

void NeedsSubView::deleteNeed(const PersonId& personId, const FamilyId& familyId)
{
    const Document& doc = DocumentManager::instance()->document();

    std::optional<Person> personOpt = doc.findPersonById(personId);
    if (!personOpt)
    {
        return;
    }

    QString message = tr("Delete special need for \"%1\"?").arg(personOpt->displayName());
    if (QMessageBox::question(this, tr("Delete Special Need"), message) == QMessageBox::Yes)
    {
        Person updatedPerson = *personOpt;
        updatedPerson.setSpecialNeedNote(QString());  // Clear the note
        updatePersonInFamily(DocumentManager::instance(), familyId, updatedPerson);
    }
}
