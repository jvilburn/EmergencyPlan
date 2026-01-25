#include "NeedsSubView.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "SpecialNeed.h"
#include "SpecialNeedCommands.h"
#include "Person.h"
#include "Phone.h"
#include "Family.h"

#include <QTreeWidget>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QLabel>

NeedsSubView::NeedsSubView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_tree(nullptr)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

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
    const QList<SpecialNeed>& needs = doc.specialNeeds();

    // Build list of (displayName, SpecialNeed) pairs for sorting
    QList<QPair<QString, SpecialNeed>> sortedNeeds;
    for (const SpecialNeed& need : needs)
    {
        QString displayName;

        if (need.personId.has_value())
        {
            auto person = doc.findPersonById(*need.personId);
            if (person)
            {
                displayName = person->displayName();
            }
            else
            {
                displayName = tr("Unknown Person");
            }
        }
        else if (need.familyId.has_value())
        {
            auto family = doc.findFamilyById(*need.familyId);
            if (family)
            {
                displayName = family->displayName();
            }
            else
            {
                displayName = tr("Unknown Family");
            }
        }
        else
        {
            // Neither person nor family - skip
            continue;
        }

        sortedNeeds.append({displayName, need});
    }

    // Sort by display name
    std::sort(sortedNeeds.begin(), sortedNeeds.end(),
              [](const auto& a, const auto& b)
              {
                  return a.first.toLower() < b.first.toLower();
              });

    // Build tree items
    for (const auto& [displayName, need] : sortedNeeds)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();

        QString text = displayName;
        if (!need.note.isEmpty())
        {
            text += QString(" - %1").arg(need.note);
        }
        item->setText(0, text);

        // Store person/family IDs for selection
        item->setData(0, PersonIdRole, need.personId.value_or(QString()));
        item->setData(0, FamilyIdRole, need.familyId.value_or(QString()));

        // Bold if selected
        bool isSelected = false;
        if (!m_selectedPersonId.isEmpty() && need.personId.has_value()
            && *need.personId == m_selectedPersonId)
        {
            isSelected = true;
        }
        else if (!m_selectedFamilyId.isEmpty() && need.familyId.has_value()
                 && *need.familyId == m_selectedFamilyId)
        {
            isSelected = true;
        }

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
    // Clear all selections
    m_selectedPersonId.clear();
    m_selectedFamilyId.clear();

    QString personId = item->data(0, PersonIdRole).toString();
    QString familyId = item->data(0, FamilyIdRole).toString();

    if (!personId.isEmpty())
    {
        m_selectedPersonId = personId;
    }
    else if (!familyId.isEmpty())
    {
        m_selectedFamilyId = familyId;
    }

    rebuildTree();
    emit highlightChanged();
}

void NeedsSubView::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);

    QMenu menu;

    if (!item)
    {
        // No item - show add option
        menu.addAction(tr("Add Special Need..."), this, &NeedsSubView::showAddNeedDialog);
    }
    else
    {
        QString personId = item->data(0, PersonIdRole).toString();
        QString familyId = item->data(0, FamilyIdRole).toString();

        const Document& doc = m_documentManager->document();

        // Show contact info (disabled) if available
        if (!personId.isEmpty())
        {
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
        }
        else if (!familyId.isEmpty())
        {
            // For family, show family contact info
            auto family = doc.findFamilyById(familyId);
            if (family)
            {
                const Phone phone = family->displayPhone();
                if (!phone.isEmpty())
                {
                    QAction* phoneAction = menu.addAction(phone);
                    phoneAction->setEnabled(false);
                }
                const QString email = family->displayEmail();
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
    case ChangeScope::SpecialNeed:
    case ChangeScope::Family:  // Person/family names
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
    const Document& doc = m_documentManager->document();

    // Clear person selection if their special need no longer exists
    if (!m_selectedPersonId.isEmpty())
    {
        auto needOpt = doc.findSpecialNeed(m_selectedPersonId, std::nullopt);
        if (!needOpt)
        {
            m_selectedPersonId.clear();
        }
    }

    // Clear family selection if their special need no longer exists
    if (!m_selectedFamilyId.isEmpty())
    {
        auto needOpt = doc.findSpecialNeed(std::nullopt, m_selectedFamilyId);
        if (!needOpt)
        {
            m_selectedFamilyId.clear();
        }
    }
}

HighlightInfo NeedsSubView::highlightInfo() const
{
    HighlightInfo info;
    const Document& doc = m_documentManager->document();

    if (!m_selectedPersonId.isEmpty())
    {
        // Person selected - highlight their family
        QString familyId = doc.familyIdForPerson(m_selectedPersonId);
        if (!familyId.isEmpty())
        {
            info.highlightedFamilyIds.insert(familyId);
        }
    }
    else if (!m_selectedFamilyId.isEmpty())
    {
        // Family selected - highlight directly
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

    // Type combo (Person or Family)
    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItem(tr("Person"));
    typeCombo->addItem(tr("Family"));
    formLayout->addRow(tr("Type:"), typeCombo);

    // Selection button and label
    QHBoxLayout* selectionLayout = new QHBoxLayout();
    QLabel* selectionLabel = new QLabel(tr("(none selected)"));
    QPushButton* selectButton = new QPushButton(tr("Select..."));
    selectionLayout->addWidget(selectionLabel, 1);
    selectionLayout->addWidget(selectButton);
    formLayout->addRow(tr("Entity:"), selectionLayout);

    // Note field
    QLineEdit* noteEdit = new QLineEdit();
    formLayout->addRow(tr("Note:"), noteEdit);

    // Dialog buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    formLayout->addRow(buttonBox);

    // Track selected ID
    QString selectedId;

    // Connect select button
    connect(selectButton, &QPushButton::clicked, &dialog, [&]()
    {
        if (typeCombo->currentIndex() == 0)
        {
            // Person mode
            QString id = WardListDialog::selectPerson(m_documentManager, selectedId, &dialog);
            if (!id.isEmpty())
            {
                selectedId = id;
                const Document& doc = m_documentManager->document();
                auto person = doc.findPersonById(id);
                if (person)
                {
                    selectionLabel->setText(person->displayName());
                }
            }
        }
        else
        {
            // Family mode
            QString id = WardListDialog::selectFamily(m_documentManager, selectedId, &dialog);
            if (!id.isEmpty())
            {
                selectedId = id;
                const Document& doc = m_documentManager->document();
                auto family = doc.findFamilyById(id);
                if (family)
                {
                    selectionLabel->setText(family->displayName());
                }
            }
        }
    });

    // When type changes, clear selection
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [&](int)
    {
        selectedId.clear();
        selectionLabel->setText(tr("(none selected)"));
    });

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted)
    {
        if (selectedId.isEmpty())
        {
            QMessageBox::warning(this, tr("Add Special Need"),
                                 tr("Please select a person or family."));
            return;
        }

        QString note = noteEdit->text().trimmed();

        if (typeCombo->currentIndex() == 0)
        {
            // Person
            m_documentManager->executeCommand(
                std::make_unique<SetSpecialNeedCommand>(
                    SetSpecialNeedCommand::forPerson(selectedId, note)));
        }
        else
        {
            // Family
            m_documentManager->executeCommand(
                std::make_unique<SetSpecialNeedCommand>(
                    SetSpecialNeedCommand::forFamily(selectedId, note)));
        }
    }
}

void NeedsSubView::showEditNeedDialog(const QString& personId, const QString& familyId)
{
    const Document& doc = m_documentManager->document();

    std::optional<QString> personIdOpt = personId.isEmpty() ? std::nullopt : std::make_optional(personId);
    std::optional<QString> familyIdOpt = familyId.isEmpty() ? std::nullopt : std::make_optional(familyId);

    auto needOpt = doc.findSpecialNeed(personIdOpt, familyIdOpt);
    if (!needOpt)
    {
        return;
    }

    // Get display name for dialog title
    QString displayName;
    if (!personId.isEmpty())
    {
        auto person = doc.findPersonById(personId);
        if (person)
        {
            displayName = person->displayName();
        }
    }
    else if (!familyId.isEmpty())
    {
        auto family = doc.findFamilyById(familyId);
        if (family)
        {
            displayName = family->displayName();
        }
    }

    bool ok;
    QString note = QInputDialog::getText(this, tr("Edit Special Need"),
                                          tr("Note for %1:").arg(displayName),
                                          QLineEdit::Normal, needOpt->note, &ok);
    if (ok)
    {
        if (!personId.isEmpty())
        {
            m_documentManager->executeCommand(
                std::make_unique<SetSpecialNeedCommand>(
                    SetSpecialNeedCommand::forPerson(personId, note)));
        }
        else
        {
            m_documentManager->executeCommand(
                std::make_unique<SetSpecialNeedCommand>(
                    SetSpecialNeedCommand::forFamily(familyId, note)));
        }
    }
}

void NeedsSubView::deleteNeed(const QString& personId, const QString& familyId)
{
    const Document& doc = m_documentManager->document();

    // Get display name for confirmation
    QString displayName;
    if (!personId.isEmpty())
    {
        auto person = doc.findPersonById(personId);
        if (person)
        {
            displayName = person->displayName();
        }
    }
    else if (!familyId.isEmpty())
    {
        auto family = doc.findFamilyById(familyId);
        if (family)
        {
            displayName = family->displayName();
        }
    }

    QString message = tr("Delete special need for \"%1\"?").arg(displayName);
    if (QMessageBox::question(this, tr("Delete Special Need"), message) == QMessageBox::Yes)
    {
        if (!personId.isEmpty())
        {
            m_documentManager->executeCommand(
                std::make_unique<ClearSpecialNeedCommand>(
                    ClearSpecialNeedCommand::forPerson(personId)));
        }
        else
        {
            m_documentManager->executeCommand(
                std::make_unique<ClearSpecialNeedCommand>(
                    ClearSpecialNeedCommand::forFamily(familyId)));
        }

        // Clear selection if we deleted the selected item
        if (m_selectedPersonId == personId && !personId.isEmpty())
        {
            m_selectedPersonId.clear();
        }
        if (m_selectedFamilyId == familyId && !familyId.isEmpty())
        {
            m_selectedFamilyId.clear();
        }
    }
}
