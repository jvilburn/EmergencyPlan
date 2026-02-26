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
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QLabel>

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

NeedsSubView::NeedsSubView(DocumentManager* documentManager,
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

    // Add button
    QPushButton* addButton = new QPushButton(tr("Add Special Need..."));
    connect(addButton, &QPushButton::clicked, this, &NeedsSubView::showAddNeedDialog);
    layout->addWidget(addButton);

    // Create model with filter from FilterBar
    m_model = new NeedsModel(documentManager, m_filterBar->filter(), this);

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

    const Document& doc = m_documentManager->document();

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

    PersonId pid = *personId;
    FamilyId fid = *familyId;
    menu.addAction(tr("Edit..."), this, [this, pid, fid]()
    {
        showEditNeedDialog(pid, fid);
    });
    menu.addAction(tr("Delete"), this, [this, pid, fid]()
    {
        deleteNeed(pid, fid);
    });

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }
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

void NeedsSubView::showAddNeedDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add Special Need"));
    dialog.setMinimumWidth(400);

    QFormLayout* formLayout = new QFormLayout(&dialog);

    // Selection button and label
    QHBoxLayout* selectionLayout = new QHBoxLayout();
    QLabel* selectionLabel = new QLabel(tr("(none selected)"));
    QPushButton* selectButton = new QPushButton(tr("Select..."));
    selectionLayout->addWidget(selectionLabel, 1);
    selectionLayout->addWidget(selectButton);
    formLayout->addRow(tr("Person:"), selectionLayout);

    // Note field
    QLineEdit* noteEdit = new QLineEdit();
    formLayout->addRow(tr("Note:"), noteEdit);

    // Dialog buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    formLayout->addRow(buttonBox);

    // Track selected person
    std::optional<PersonId> selectedPersonId;
    std::optional<FamilyId> selectedFamilyId;

    // Connect select button
    connect(selectButton, &QPushButton::clicked, &dialog, [&]()
    {
        auto id = WardListDialog::selectPerson(m_documentManager, selectedPersonId, &dialog);
        if (id)
        {
            selectedPersonId = id;
            const Document& doc = m_documentManager->document();
            std::optional<Person> person = doc.findPersonById(*id);
            if (person)
            {
                selectionLabel->setText(person->displayName());
                // Pre-fill note if person already has a special need
                if (person->hasSpecialNeed())
                {
                    noteEdit->setText(person->specialNeedNote());
                }
                selectedFamilyId = doc.familyIdForPerson(*id);
            }
        }
    });

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted)
    {
        if (!selectedPersonId)
        {
            QMessageBox::warning(this, tr("Add Special Need"),
                                 tr("Please select a person."));
            return;
        }

        QString note = noteEdit->text().trimmed();

        // Update person's special need note
        const Document& doc = m_documentManager->document();
        std::optional<Person> personOpt = doc.findPersonById(*selectedPersonId);
        if (personOpt && selectedFamilyId)
        {
            Person updatedPerson = *personOpt;
            updatedPerson.setSpecialNeedNote(note);
            updatePersonInFamily(m_documentManager, *selectedFamilyId, updatedPerson);
        }
    }
}

void NeedsSubView::showEditNeedDialog(const PersonId& personId, const FamilyId& familyId)
{
    const Document& doc = m_documentManager->document();

    std::optional<Person> personOpt = doc.findPersonById(personId);
    if (!personOpt || !personOpt->hasSpecialNeed())
    {
        return;
    }

    bool ok;
    QString note = QInputDialog::getText(this, tr("Edit Special Need"),
                                          tr("Note for %1:").arg(personOpt->displayName()),
                                          QLineEdit::Normal, personOpt->specialNeedNote(), &ok);
    if (ok)
    {
        Person updatedPerson = *personOpt;
        updatedPerson.setSpecialNeedNote(note);
        updatePersonInFamily(m_documentManager, familyId, updatedPerson);
    }
}

void NeedsSubView::deleteNeed(const PersonId& personId, const FamilyId& familyId)
{
    const Document& doc = m_documentManager->document();

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
        updatePersonInFamily(m_documentManager, familyId, updatedPerson);
    }
}
