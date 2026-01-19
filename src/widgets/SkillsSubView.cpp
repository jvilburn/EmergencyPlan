#include "SkillsSubView.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Skill.h"
#include "SkillCategory.h"
#include "Person.h"
#include "Phone.h"
#include "Family.h"
#include "SkillCommands.h"

#include <QTreeWidget>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

SkillsSubView::SkillsSubView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_tree(nullptr)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);

    m_tree->setStyleSheet(R"(
        QTreeWidget {
            border: 1px solid #e0e0e0;
            border-radius: 4px;
            background: white;
        }
        QTreeWidget::item {
            padding: 4px 0;
        }
        QTreeWidget::item:hover {
            background: #f5f5f5;
        }
        QTreeWidget::item:selected {
            background: #e3f2fd;
            color: black;
        }
    )");

    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked,
            this, &SkillsSubView::onTreeItemClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &SkillsSubView::onTreeItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &SkillsSubView::onContextMenu);
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &SkillsSubView::onDocumentChanged);

    rebuildTree();
}

void SkillsSubView::rebuildTree()
{
    m_tree->clear();

    const Document& doc = m_documentManager->document();
    const auto& categories = doc.skillCategories();
    const auto& skills = doc.skills();

    // Sort categories by sortOrder then name
    QList<SkillCategory> sortedCategories = categories.values();
    std::sort(sortedCategories.begin(), sortedCategories.end(),
              [](const SkillCategory& a, const SkillCategory& b)
              {
                  if (a.sortOrder() != b.sortOrder())
                  {
                      return a.sortOrder() < b.sortOrder();
                  }
                  return a.name().toLower() < b.name().toLower();
              });

    for (const SkillCategory& category : sortedCategories)
    {
        // Get skills in this category
        QList<Skill> categorySkills;
        for (const auto& skill : skills)
        {
            if (skill.categoryId() == category.id())
            {
                categorySkills.append(skill);
            }
        }

        // Sort skills by name
        std::sort(categorySkills.begin(), categorySkills.end(),
                  [](const Skill& a, const Skill& b)
                  {
                      return a.name().toLower() < b.name().toLower();
                  });

        // Count total persons in category
        int totalPersons = 0;
        for (const Skill& skill : categorySkills)
        {
            totalPersons += skill.personIds().size();
        }

        // Create category item
        auto* categoryItem = new QTreeWidgetItem();
        categoryItem->setText(0, QString("%1 (%2)").arg(category.name()).arg(totalPersons));
        categoryItem->setData(0, IdRole, category.id());
        categoryItem->setData(0, TypeRole, static_cast<int>(ItemType::Category));

        // Bold if selected
        if (m_selectedCategoryId == category.id())
        {
            QFont font = categoryItem->font(0);
            font.setBold(true);
            categoryItem->setFont(0, font);
        }

        // Add skills under this category
        for (const Skill& skill : categorySkills)
        {
            auto* skillItem = new QTreeWidgetItem(categoryItem);
            skillItem->setText(0, QString("%1 (%2)").arg(skill.name()).arg(skill.personIds().size()));
            skillItem->setData(0, IdRole, skill.id());
            skillItem->setData(0, TypeRole, static_cast<int>(ItemType::Skill));

            // Bold if selected
            if (m_selectedSkillId == skill.id())
            {
                QFont font = skillItem->font(0);
                font.setBold(true);
                skillItem->setFont(0, font);
            }

            // Get persons with this skill, sorted by display name
            QList<QPair<QString, QString>> persons;  // (personId, displayName)
            for (const QString& personId : skill.personIds())
            {
                auto person = doc.findPersonById(personId);
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
                auto* personItem = new QTreeWidgetItem(skillItem);
                personItem->setText(0, displayName);
                personItem->setData(0, IdRole, personId);
                personItem->setData(0, TypeRole, static_cast<int>(ItemType::Person));
                personItem->setData(0, SecondaryIdRole, skill.id());

                // Bold if selected
                if (m_selectedPersonId == personId)
                {
                    QFont font = personItem->font(0);
                    font.setBold(true);
                    personItem->setFont(0, font);
                }
            }
        }

        m_tree->addTopLevelItem(categoryItem);
        categoryItem->setExpanded(true);
    }
}

void SkillsSubView::onTreeItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    // Clear all selections
    m_selectedCategoryId.clear();
    m_selectedSkillId.clear();
    m_selectedPersonId.clear();

    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    switch (type)
    {
    case ItemType::Category:
        m_selectedCategoryId = id;
        break;

    case ItemType::Skill:
        m_selectedSkillId = id;
        break;

    case ItemType::Person:
        m_selectedPersonId = id;
        m_selectedSkillId = item->data(0, SecondaryIdRole).toString();
        break;
    }

    rebuildTree();
    emit highlightChanged();
}

void SkillsSubView::onTreeItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    switch (type)
    {
    case ItemType::Category:
    case ItemType::Skill:
        showRenameDialog(item);
        break;

    case ItemType::Person:
        // Could open person details in future
        break;
    }
}

void SkillsSubView::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);

    QMenu menu;

    if (!item)
    {
        // No item - show add category option
        menu.addAction(tr("Add Category..."), this, &SkillsSubView::showAddCategoryDialog);
    }
    else
    {
        ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
        QString id = item->data(0, IdRole).toString();

        switch (type)
        {
        case ItemType::Category:
            {
                const Document& doc = m_documentManager->document();
                auto categoryOpt = doc.skillCategories().value(id);
                QString categoryName = categoryOpt.name();

                menu.addAction(tr("Rename..."), this, [this, item]()
                {
                    showRenameDialog(item);
                });
                menu.addAction(tr("Add %1 Skill...").arg(categoryName), this, [this, id]()
                {
                    showAddSkillDialog(id);
                });
                menu.addSeparator();
                menu.addAction(tr("Delete"), this, [this, item]()
                {
                    deleteItem(item);
                });
            }
            break;

        case ItemType::Skill:
            {
                menu.addAction(tr("Select People..."), this, [this, id]()
                {
                    showSelectPeopleDialog(id);
                });
                menu.addSeparator();
                menu.addAction(tr("Rename..."), this, [this, item]()
                {
                    showRenameDialog(item);
                });
                menu.addAction(tr("Delete"), this, [this, item]()
                {
                    deleteItem(item);
                });
            }
            break;

        case ItemType::Person:
            {
                // Show contact info (disabled) if available
                const Document& doc = m_documentManager->document();
                auto personOpt = doc.findPersonById(id);
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

                QString skillId = item->data(0, SecondaryIdRole).toString();
                menu.addAction(tr("Remove"), this, [this, skillId, id]()
                {
                    removePersonFromSkill(skillId, id);
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

void SkillsSubView::onDocumentChanged()
{
    rebuildTree();
    emit highlightChanged();
}

HighlightInfo SkillsSubView::highlightInfo() const
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
    else if (!m_selectedSkillId.isEmpty())
    {
        // Skill selected - highlight all families with people having this skill
        auto skillOpt = doc.findSkillById(m_selectedSkillId);
        if (skillOpt)
        {
            for (const QString& personId : skillOpt->personIds())
            {
                QString familyId = doc.familyIdForPerson(personId);
                if (!familyId.isEmpty())
                {
                    info.highlightedFamilyIds.insert(familyId);
                }
            }
        }
    }
    else if (!m_selectedCategoryId.isEmpty())
    {
        // Category selected - highlight all families in all skills in this category
        const auto& skills = doc.skills();
        for (const auto& skill : skills)
        {
            if (skill.categoryId() == m_selectedCategoryId)
            {
                for (const QString& personId : skill.personIds())
                {
                    QString familyId = doc.familyIdForPerson(personId);
                    if (!familyId.isEmpty())
                    {
                        info.highlightedFamilyIds.insert(familyId);
                    }
                }
            }
        }
    }

    return info;
}

QSet<QString> SkillsSubView::visibleFamilyIds() const
{
    // Show all families
    return {};
}

void SkillsSubView::showAddCategoryDialog()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Category"),
                                          tr("Category name:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty())
    {
        const Document& doc = m_documentManager->document();
        const auto& categories = doc.skillCategories();

        // Find max sort order
        int maxOrder = 0;
        for (const auto& category : categories)
        {
            if (category.sortOrder() > maxOrder)
            {
                maxOrder = category.sortOrder();
            }
        }

        SkillCategory newCategory = SkillCategory::create(name, maxOrder + 1);
        m_documentManager->executeCommand(std::make_unique<AddSkillCategoryCommand>(newCategory));
    }
}

void SkillsSubView::showAddSkillDialog(const QString& categoryId)
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Skill"),
                                          tr("Skill name:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty())
    {
        Skill newSkill = Skill::create(name, categoryId);
        m_documentManager->executeCommand(std::make_unique<AddSkillCommand>(newSkill));
    }
}

void SkillsSubView::showRenameDialog(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    const Document& doc = m_documentManager->document();

    if (type == ItemType::Category)
    {
        auto categoryOpt = doc.skillCategories().value(id);
        bool ok;
        QString name = QInputDialog::getText(this, tr("Rename Category"),
                                              tr("Category name:"),
                                              QLineEdit::Normal, categoryOpt.name(), &ok);
        if (ok && !name.isEmpty() && name != categoryOpt.name())
        {
            SkillCategory updated = categoryOpt;
            updated.setName(name);
            m_documentManager->executeCommand(
                std::make_unique<UpdateSkillCategoryCommand>(categoryOpt, updated));
        }
    }
    else if (type == ItemType::Skill)
    {
        auto skillOpt = doc.findSkillById(id);
        if (skillOpt)
        {
            bool ok;
            QString name = QInputDialog::getText(this, tr("Rename Skill"),
                                                  tr("Skill name:"),
                                                  QLineEdit::Normal, skillOpt->name(), &ok);
            if (ok && !name.isEmpty() && name != skillOpt->name())
            {
                Skill updated = *skillOpt;
                updated.setName(name);
                m_documentManager->executeCommand(
                    std::make_unique<UpdateSkillCommand>(*skillOpt, updated));
            }
        }
    }
}

void SkillsSubView::showSelectPeopleDialog(const QString& skillId)
{
    const Document& doc = m_documentManager->document();
    auto skillOpt = doc.findSkillById(skillId);
    if (!skillOpt)
    {
        return;
    }

    // Get current person IDs as a list
    QStringList currentIds = skillOpt->personIds().values();

    // Show dialog to select persons
    QStringList selectedIds = WardListDialog::selectPersons(m_documentManager, currentIds, this);

    // Determine additions and removals
    QSet<QString> currentSet = skillOpt->personIds();
    QSet<QString> selectedSet(selectedIds.begin(), selectedIds.end());

    QSet<QString> toAdd = selectedSet - currentSet;
    QSet<QString> toRemove = currentSet - selectedSet;

    // Execute commands for changes
    for (const QString& personId : toAdd)
    {
        m_documentManager->executeCommand(
            std::make_unique<AssignSkillToPersonCommand>(skillId, personId));
    }

    for (const QString& personId : toRemove)
    {
        m_documentManager->executeCommand(
            std::make_unique<UnassignSkillFromPersonCommand>(skillId, personId));
    }
}

void SkillsSubView::deleteItem(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    const Document& doc = m_documentManager->document();

    if (type == ItemType::Category)
    {
        auto categoryOpt = doc.skillCategories().value(id);

        // Count skills in this category
        int skillCount = 0;
        for (const auto& skill : doc.skills())
        {
            if (skill.categoryId() == id)
            {
                skillCount++;
            }
        }

        QString message = skillCount > 0
            ? tr("Delete category \"%1\" and its %2 skill(s)?").arg(categoryOpt.name()).arg(skillCount)
            : tr("Delete category \"%1\"?").arg(categoryOpt.name());

        if (QMessageBox::question(this, tr("Delete Category"), message) == QMessageBox::Yes)
        {
            // Delete skills in this category first
            for (const auto& skill : doc.skills())
            {
                if (skill.categoryId() == id)
                {
                    m_documentManager->executeCommand(
                        std::make_unique<DeleteSkillCommand>(skill));
                }
            }

            // Then delete the category
            m_documentManager->executeCommand(
                std::make_unique<DeleteSkillCategoryCommand>(categoryOpt));
        }
    }
    else if (type == ItemType::Skill)
    {
        auto skillOpt = doc.findSkillById(id);
        if (skillOpt)
        {
            QString message = tr("Delete skill \"%1\"?").arg(skillOpt->name());
            if (QMessageBox::question(this, tr("Delete Skill"), message) == QMessageBox::Yes)
            {
                m_documentManager->executeCommand(
                    std::make_unique<DeleteSkillCommand>(*skillOpt));
            }
        }
    }
}

void SkillsSubView::removePersonFromSkill(const QString& skillId, const QString& personId)
{
    m_documentManager->executeCommand(
        std::make_unique<UnassignSkillFromPersonCommand>(skillId, personId));
}
