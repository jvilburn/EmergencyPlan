#include "NeedsSubView.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Person.h"
#include "Phone.h"
#include "Family.h"
#include "FamilyCommands.h"

#include <QTreeWidget>
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
void updatePersonInFamily(DocumentManager* docMgr, const QString& familyId, const Person& updatedPerson)
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

NeedsSubView::NeedsSubView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_tree(nullptr)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    // Add button at top
    QPushButton* addButton = new QPushButton(tr("Add Special Need..."));
    connect(addButton, &QPushButton::clicked, this, &NeedsSubView::showAddNeedDialog);
    layout->addWidget(addButton);

    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);  // Flat list, no expand/collapse indicators
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked,
            this, &NeedsSubView::onTreeItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &NeedsSubView::onContextMenu);
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &NeedsSubView::onDocumentChanged);

    rebuildTree();
}

void NeedsSubView::rebuildTree()
{
    m_tree->clear();

    const Document& doc = m_documentManager->document();

    // Build list of entries for sorting
    struct NeedEntry
    {
        QString displayName;
        QString personId;
        QString familyId;
        QString note;
    };
    QList<NeedEntry> entries;

    // Iterate through all families and find persons with special needs
    for (const Family& family : doc.families())
    {
        for (const Person& person : family.members())
        {
            if (person.hasSpecialNeed())
            {
                entries.append({
                    person.displayName(),
                    person.id(),
                    family.id(),
                    person.specialNeedNote()
                });
            }
        }
    }

    // Sort by display name
    std::sort(entries.begin(), entries.end(),
              [](const NeedEntry& a, const NeedEntry& b)
              {
                  return a.displayName.toLower() < b.displayName.toLower();
              });

    // Build tree items
    for (const NeedEntry& entry : entries)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();

        QString text = entry.displayName;
        if (!entry.note.isEmpty())
        {
            text += QString(" - %1").arg(entry.note);
        }
        item->setText(0, text);

        // Store person and family IDs for selection and lookup
        item->setData(0, PersonIdRole, entry.personId);
        item->setData(0, FamilyIdRole, entry.familyId);

        // Bold if selected
        bool isSelected = (!m_selectedPersonId.isEmpty()
                           && entry.personId == m_selectedPersonId);

        if (isSelected)
        {
            QFont font = item->font(0);
            font.setBold(true);
            item->setFont(0, font);
        }

        m_tree->addTopLevelItem(item);
    }
}

void NeedsSubView::onTreeItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    m_selectedPersonId = item->data(0, PersonIdRole).toString();
    m_selectedFamilyId = item->data(0, FamilyIdRole).toString();

    rebuildTree();
    emit highlightChanged();
}

void NeedsSubView::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item)
    {
        return;  // No context menu on empty space (use the Add button instead)
    }

    QMenu menu;
    {
        QString personId = item->data(0, PersonIdRole).toString();
        QString familyId = item->data(0, FamilyIdRole).toString();

        const Document& doc = m_documentManager->document();

        // Show contact info (disabled) if available
        auto person = doc.findPersonById(personId);
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

        menu.addAction(tr("Edit..."), this, [this, personId, familyId]()
        {
            showEditNeedDialog(personId, familyId);
        });
        menu.addAction(tr("Delete"), this, [this, personId, familyId]()
        {
            deleteNeed(personId, familyId);
        });
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }
}

void NeedsSubView::onDocumentChanged(const DocumentChange& change)
{
    switch (change.scope)
    {
    case ChangeScope::Full:
    case ChangeScope::Family:  // Persons are in families
        validateSelections();
        rebuildTree();
        emit highlightChanged();
        break;
    default:
        break;
    }
}

void NeedsSubView::validateSelections()
{
    if (m_selectedPersonId.isEmpty())
    {
        return;
    }

    const Document& doc = m_documentManager->document();

    // Clear selection if person no longer has a special need
    auto person = doc.findPersonById(m_selectedPersonId);
    if (!person || !person->hasSpecialNeed())
    {
        m_selectedPersonId.clear();
        m_selectedFamilyId.clear();
    }
}

HighlightInfo NeedsSubView::highlightInfo() const
{
    HighlightInfo info;

    if (!m_selectedFamilyId.isEmpty())
    {
        info.highlightedFamilyIds.insert(m_selectedFamilyId);
    }

    return info;
}

QSet<QString> NeedsSubView::visibleFamilyIds() const
{
    // Show all families
    return {};
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
    QString selectedPersonId;
    QString selectedFamilyId;

    // Connect select button
    connect(selectButton, &QPushButton::clicked, &dialog, [&]()
    {
        QString id = WardListDialog::selectPerson(m_documentManager, selectedPersonId, &dialog);
        if (!id.isEmpty())
        {
            selectedPersonId = id;
            const Document& doc = m_documentManager->document();
            auto person = doc.findPersonById(id);
            if (person)
            {
                selectionLabel->setText(person->displayName());
                // Pre-fill note if person already has a special need
                if (person->hasSpecialNeed())
                {
                    noteEdit->setText(person->specialNeedNote());
                }
                selectedFamilyId = doc.familyIdForPerson(id);
            }
        }
    });

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted)
    {
        if (selectedPersonId.isEmpty())
        {
            QMessageBox::warning(this, tr("Add Special Need"),
                                 tr("Please select a person."));
            return;
        }

        QString note = noteEdit->text().trimmed();

        // Update person's special need note
        const Document& doc = m_documentManager->document();
        auto personOpt = doc.findPersonById(selectedPersonId);
        if (personOpt)
        {
            Person updatedPerson = *personOpt;
            updatedPerson.setSpecialNeedNote(note);
            updatePersonInFamily(m_documentManager, selectedFamilyId, updatedPerson);
        }
    }
}

void NeedsSubView::showEditNeedDialog(const QString& personId, const QString& familyId)
{
    const Document& doc = m_documentManager->document();

    auto personOpt = doc.findPersonById(personId);
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

void NeedsSubView::deleteNeed(const QString& personId, const QString& familyId)
{
    const Document& doc = m_documentManager->document();

    auto personOpt = doc.findPersonById(personId);
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

        // Clear selection if we deleted the selected item
        if (m_selectedPersonId == personId)
        {
            m_selectedPersonId.clear();
            m_selectedFamilyId.clear();
        }
    }
}
