# Emergency Tab Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement the Emergency sidebar tab with Skills, Equipment, and Needs sub-tabs, each with 3-level tree hierarchy and map highlighting.

**Architecture:** EmergencyView contains QTabWidget with sub-views. Each sub-view (SkillsSubView, EquipmentSubView, NeedsSubView) inherits from QWidget and MapHighlightProvider, following MinisteringView pattern. Sub-views forward highlightChanged signal to parent EmergencyView.

**Tech Stack:** Qt 6 Widgets, QTreeWidget, QTabWidget, existing WardListDialog, existing Document methods

**Design Reference:** [2026-01-18-emergency-tab-design.md](2026-01-18-emergency-tab-design.md)

---

## Task 1: Create EmergencyView Container

**Files:**
- Create: `src/widgets/EmergencyView.h`
- Create: `src/widgets/EmergencyView.cpp`
- Modify: `src/widgets/CMakeLists.txt` (add to sources)

### Step 1: Create EmergencyView.h

```cpp
#ifndef EMERGENCYVIEW_H
#define EMERGENCYVIEW_H

#include <QWidget>
#include "MapHighlightProvider.h"

class QTabWidget;
class DocumentManager;
class SkillsSubView;
class EquipmentSubView;
class NeedsSubView;

class EmergencyView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT

public:
    explicit EmergencyView(DocumentManager* documentManager, QWidget* parent = nullptr);

    // MapHighlightProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onSubTabChanged(int index);

private:
    DocumentManager* m_documentManager;
    QTabWidget* m_subTabs;
    SkillsSubView* m_skillsView;
    EquipmentSubView* m_equipmentView;
    NeedsSubView* m_needsView;

    MapHighlightProvider* currentSubProvider() const;
};

#endif // EMERGENCYVIEW_H
```

### Step 2: Create EmergencyView.cpp

```cpp
#include "EmergencyView.h"
#include "SkillsSubView.h"
#include "EquipmentSubView.h"
#include "NeedsSubView.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>

EmergencyView::EmergencyView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_subTabs = new QTabWidget();

    // Placeholder for Teams tab (future)
    QWidget* teamsPlaceholder = new QWidget();
    QVBoxLayout* teamsLayout = new QVBoxLayout(teamsPlaceholder);
    QLabel* teamsLabel = new QLabel(tr("Teams functionality coming soon"));
    teamsLabel->setAlignment(Qt::AlignCenter);
    teamsLayout->addWidget(teamsLabel);
    m_subTabs->addTab(teamsPlaceholder, tr("Teams"));

    m_skillsView = new SkillsSubView(documentManager);
    m_subTabs->addTab(m_skillsView, tr("Skills"));

    m_equipmentView = new EquipmentSubView(documentManager);
    m_subTabs->addTab(m_equipmentView, tr("Equipment"));

    m_needsView = new NeedsSubView(documentManager);
    m_subTabs->addTab(m_needsView, tr("Needs"));

    layout->addWidget(m_subTabs);

    // Forward highlight signals from sub-views
    connect(m_skillsView, &SkillsSubView::highlightChanged,
            this, &EmergencyView::highlightChanged);
    connect(m_equipmentView, &EquipmentSubView::highlightChanged,
            this, &EmergencyView::highlightChanged);
    connect(m_needsView, &NeedsSubView::highlightChanged,
            this, &EmergencyView::highlightChanged);

    connect(m_subTabs, &QTabWidget::currentChanged,
            this, &EmergencyView::onSubTabChanged);

    // Start on Skills tab (index 1, after Teams placeholder)
    m_subTabs->setCurrentIndex(1);
}

MapHighlightProvider* EmergencyView::currentSubProvider() const
{
    QWidget* current = m_subTabs->currentWidget();
    return dynamic_cast<MapHighlightProvider*>(current);
}

HighlightInfo EmergencyView::highlightInfo() const
{
    MapHighlightProvider* provider = currentSubProvider();
    if (provider)
    {
        return provider->highlightInfo();
    }
    return HighlightInfo{};
}

QSet<QString> EmergencyView::visibleFamilyIds() const
{
    MapHighlightProvider* provider = currentSubProvider();
    if (provider)
    {
        return provider->visibleFamilyIds();
    }
    return {};
}

void EmergencyView::onSubTabChanged(int index)
{
    Q_UNUSED(index);
    emit highlightChanged();
}
```

### Step 3: Update CMakeLists.txt

Add to `src/widgets/CMakeLists.txt` in the sources list:
```
EmergencyView.cpp
EmergencyView.h
```

### Step 4: Build and verify

Run: `build.bat`
Expected: Build fails with missing SkillsSubView, EquipmentSubView, NeedsSubView headers (expected - we'll create them next)

---

## Task 2: Create SkillsSubView

**Files:**
- Create: `src/widgets/SkillsSubView.h`
- Create: `src/widgets/SkillsSubView.cpp`
- Modify: `src/widgets/CMakeLists.txt`

### Step 1: Create SkillsSubView.h

```cpp
#ifndef SKILLSSUBVIEW_H
#define SKILLSSUBVIEW_H

#include <QWidget>
#include <QSet>
#include "MapHighlightProvider.h"

class QTreeWidget;
class QTreeWidgetItem;
class DocumentManager;
class DocumentChange;

class SkillsSubView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT

public:
    explicit SkillsSubView(DocumentManager* documentManager, QWidget* parent = nullptr);

    // MapHighlightProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onDocumentChanged(const DocumentChange& change);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);

private:
    void rebuildTree();
    void showAddCategoryDialog();
    void showAddSkillDialog(const QString& categoryId);
    void showRenameDialog(QTreeWidgetItem* item);
    void showSelectPeopleDialog(const QString& skillId);
    void deleteItem(QTreeWidgetItem* item);
    void removePersonFromSkill(const QString& skillId, const QString& personId);

    static constexpr int IdRole = Qt::UserRole;
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SecondaryIdRole = Qt::UserRole + 2;  // For person's skillId
    enum class ItemType { Category, Skill, Person };

    DocumentManager* m_documentManager;
    QTreeWidget* m_tree;
    QString m_selectedCategoryId;
    QString m_selectedSkillId;
    QString m_selectedPersonId;
};

#endif // SKILLSSUBVIEW_H
```

### Step 2: Create SkillsSubView.cpp (part 1 - constructor and tree building)

```cpp
#include "SkillsSubView.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Skill.h"
#include "SkillCategory.h"
#include "Person.h"
#include "Family.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

SkillsSubView::SkillsSubView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
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

void SkillsSubView::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change);
    // TODO: Optimize for specific scopes
    rebuildTree();
}

void SkillsSubView::rebuildTree()
{
    m_tree->clear();

    const Document& doc = m_documentManager->document();
    const QHash<QString, SkillCategory>& categories = doc.skillCategories();
    const QHash<QString, Skill>& skills = doc.skills();

    // Sort categories by sortOrder then name
    QList<SkillCategory> sortedCategories = categories.values();
    std::sort(sortedCategories.begin(), sortedCategories.end(),
              [](const SkillCategory& a, const SkillCategory& b) {
                  if (a.sortOrder() != b.sortOrder())
                  {
                      return a.sortOrder() < b.sortOrder();
                  }
                  return a.name().toLower() < b.name().toLower();
              });

    for (const SkillCategory& category : sortedCategories)
    {
        // Count total persons in this category
        int totalPersons = 0;
        QList<Skill> categorySkills;
        for (const Skill& skill : skills)
        {
            if (skill.categoryId() == category.id())
            {
                categorySkills.append(skill);
                totalPersons += skill.personIds().size();
            }
        }

        // Sort skills by name
        std::sort(categorySkills.begin(), categorySkills.end(),
                  [](const Skill& a, const Skill& b) {
                      return a.name().toLower() < b.name().toLower();
                  });

        // Create category item
        QTreeWidgetItem* categoryItem = new QTreeWidgetItem();
        categoryItem->setText(0, QString("%1 (%2)").arg(category.name()).arg(totalPersons));
        categoryItem->setData(0, IdRole, category.id());
        categoryItem->setData(0, TypeRole, static_cast<int>(ItemType::Category));

        if (category.id() == m_selectedCategoryId)
        {
            QFont font = categoryItem->font(0);
            font.setBold(true);
            categoryItem->setFont(0, font);
        }

        // Add skills under category
        for (const Skill& skill : categorySkills)
        {
            QTreeWidgetItem* skillItem = new QTreeWidgetItem(categoryItem);
            skillItem->setText(0, QString("%1 (%2)").arg(skill.name()).arg(skill.personIds().size()));
            skillItem->setData(0, IdRole, skill.id());
            skillItem->setData(0, TypeRole, static_cast<int>(ItemType::Skill));

            if (skill.id() == m_selectedSkillId)
            {
                QFont font = skillItem->font(0);
                font.setBold(true);
                skillItem->setFont(0, font);
            }

            // Add persons under skill
            QList<QString> personIds = skill.personIds().values();
            // Sort persons by name
            std::sort(personIds.begin(), personIds.end(),
                      [&doc](const QString& a, const QString& b) {
                          std::optional<Person> personA = doc.findPersonById(a);
                          std::optional<Person> personB = doc.findPersonById(b);
                          QString nameA = personA ? personA->displayName() : a;
                          QString nameB = personB ? personB->displayName() : b;
                          return nameA.toLower() < nameB.toLower();
                      });

            for (const QString& personId : personIds)
            {
                std::optional<Person> person = doc.findPersonById(personId);
                if (!person)
                {
                    continue;
                }

                QTreeWidgetItem* personItem = new QTreeWidgetItem(skillItem);
                personItem->setText(0, person->displayName());
                personItem->setData(0, IdRole, personId);
                personItem->setData(0, TypeRole, static_cast<int>(ItemType::Person));
                personItem->setData(0, SecondaryIdRole, skill.id());

                if (personId == m_selectedPersonId)
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
```

### Step 3: Create SkillsSubView.cpp (part 2 - click handlers and highlighting)

Append to SkillsSubView.cpp:

```cpp
void SkillsSubView::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

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

void SkillsSubView::onTreeItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    // Double-click on person shows contact details (expand to see children)
    // For now, just expand/collapse (default behavior)
    // Contact details can be added as children in future enhancement
    if (type == ItemType::Person)
    {
        // Future: could expand to show contact info as children
    }
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
        // Skill selected - highlight all assigned persons' families
        Skill skill = doc.skills().value(m_selectedSkillId);
        for (const QString& personId : skill.personIds())
        {
            QString familyId = doc.familyIdForPerson(personId);
            if (!familyId.isEmpty())
            {
                info.highlightedFamilyIds.insert(familyId);
            }
        }
    }
    else if (!m_selectedCategoryId.isEmpty())
    {
        // Category selected - highlight all persons in all skills in this category
        for (const Skill& skill : doc.skills())
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
    // Show all families (empty set means all visible)
    return {};
}
```

### Step 4: Create SkillsSubView.cpp (part 3 - context menu and dialogs)

Append to SkillsSubView.cpp:

```cpp
void SkillsSubView::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    QMenu menu(this);

    if (!item)
    {
        // Empty area - offer to add category
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
            menu.addAction(tr("Rename..."), this, [this, item]() { showRenameDialog(item); });

            // Get category name for dynamic label
            const Document& doc = m_documentManager->document();
            SkillCategory cat = doc.skillCategories().value(id);
            QString addLabel = tr("Add %1 Skill...").arg(cat.name());
            menu.addAction(addLabel, this, [this, id]() { showAddSkillDialog(id); });

            menu.addSeparator();
            menu.addAction(tr("Delete"), this, [this, item]() { deleteItem(item); });
            break;
        }
        case ItemType::Skill:
        {
            menu.addAction(tr("Select People..."), this, [this, id]() { showSelectPeopleDialog(id); });
            menu.addSeparator();
            menu.addAction(tr("Rename..."), this, [this, item]() { showRenameDialog(item); });
            menu.addAction(tr("Delete"), this, [this, item]() { deleteItem(item); });
            break;
        }
        case ItemType::Person:
        {
            QString skillId = item->data(0, SecondaryIdRole).toString();

            // Show contact info (disabled)
            const Document& doc = m_documentManager->document();
            std::optional<Person> person = doc.findPersonById(id);
            if (person)
            {
                QString familyId = doc.familyIdForPerson(id);
                std::optional<Family> family = doc.findFamilyById(familyId);

                if (family && !family->address().isEmpty())
                {
                    QAction* addrAction = menu.addAction(family->address());
                    addrAction->setEnabled(false);
                }
                if (!person->phone().isEmpty())
                {
                    QAction* phoneAction = menu.addAction(person->phone());
                    phoneAction->setEnabled(false);
                }
                if (!person->email().isEmpty())
                {
                    QAction* emailAction = menu.addAction(person->email());
                    emailAction->setEnabled(false);
                }
                if (menu.actions().size() > 0)
                {
                    menu.addSeparator();
                }
            }

            menu.addAction(tr("Remove"), this, [this, skillId, id]() { removePersonFromSkill(skillId, id); });
            break;
        }
        }
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }
}

void SkillsSubView::showAddCategoryDialog()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Category"),
                                         tr("Category name:"), QLineEdit::Normal,
                                         QString(), &ok);
    if (ok && !name.isEmpty())
    {
        Document& doc = m_documentManager->document();
        int maxOrder = 0;
        for (const SkillCategory& cat : doc.skillCategories())
        {
            maxOrder = std::max(maxOrder, cat.sortOrder());
        }

        SkillCategory category = SkillCategory::create(name, maxOrder + 1);
        m_documentManager->document().addSkillCategory(category);
        m_documentManager->markDirty();
    }
}

void SkillsSubView::showAddSkillDialog(const QString& categoryId)
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Skill"),
                                         tr("Skill name:"), QLineEdit::Normal,
                                         QString(), &ok);
    if (ok && !name.isEmpty())
    {
        Skill skill = Skill::create(name, categoryId);
        m_documentManager->document().addSkill(skill);
        m_documentManager->markDirty();
    }
}

void SkillsSubView::showRenameDialog(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    bool ok;
    QString currentName;
    QString title;
    QString label;

    if (type == ItemType::Category)
    {
        const Document& doc = m_documentManager->document();
        SkillCategory cat = doc.skillCategories().value(id);
        currentName = cat.name();
        title = tr("Rename Category");
        label = tr("Category name:");
    }
    else if (type == ItemType::Skill)
    {
        const Document& doc = m_documentManager->document();
        Skill skill = doc.skills().value(id);
        currentName = skill.name();
        title = tr("Rename Skill");
        label = tr("Skill name:");
    }
    else
    {
        return;
    }

    QString newName = QInputDialog::getText(this, title, label,
                                            QLineEdit::Normal, currentName, &ok);
    if (ok && !newName.isEmpty() && newName != currentName)
    {
        Document& doc = m_documentManager->document();
        if (type == ItemType::Category)
        {
            SkillCategory cat = doc.skillCategories().value(id);
            cat.setName(newName);
            doc.updateSkillCategory(cat);
        }
        else
        {
            Skill skill = doc.skills().value(id);
            skill.setName(newName);
            doc.updateSkill(skill);
        }
        m_documentManager->markDirty();
    }
}

void SkillsSubView::showSelectPeopleDialog(const QString& skillId)
{
    const Document& doc = m_documentManager->document();
    Skill skill = doc.skills().value(skillId);

    QStringList currentIds = skill.personIds().values();
    QStringList selectedIds = WardListDialog::selectPersons(
        m_documentManager, currentIds, this);

    // Compare and update
    QSet<QString> oldSet(currentIds.begin(), currentIds.end());
    QSet<QString> newSet(selectedIds.begin(), selectedIds.end());

    // Remove unselected
    for (const QString& personId : oldSet)
    {
        if (!newSet.contains(personId))
        {
            m_documentManager->document().removePersonFromSkill(skillId, personId);
        }
    }

    // Add newly selected
    for (const QString& personId : newSet)
    {
        if (!oldSet.contains(personId))
        {
            m_documentManager->document().addPersonToSkill(skillId, personId);
        }
    }

    if (oldSet != newSet)
    {
        m_documentManager->markDirty();
    }
}

void SkillsSubView::deleteItem(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    if (type == ItemType::Category)
    {
        const Document& doc = m_documentManager->document();
        SkillCategory cat = doc.skillCategories().value(id);

        QMessageBox::StandardButton result = QMessageBox::question(this, tr("Delete Category"),
            tr("Delete category '%1' and all its skills?").arg(cat.name()),
            QMessageBox::Yes | QMessageBox::No);

        if (result == QMessageBox::Yes)
        {
            // First delete all skills in this category
            Document& docMut = m_documentManager->document();
            QList<QString> skillsToDelete;
            for (const Skill& skill : docMut.skills())
            {
                if (skill.categoryId() == id)
                {
                    skillsToDelete.append(skill.id());
                }
            }
            for (const QString& skillIdToDelete : skillsToDelete)
            {
                docMut.removeSkill(skillIdToDelete);
            }
            docMut.removeSkillCategory(id);
            m_documentManager->markDirty();
        }
    }
    else if (type == ItemType::Skill)
    {
        const Document& doc = m_documentManager->document();
        Skill skill = doc.skills().value(id);

        QMessageBox::StandardButton result = QMessageBox::question(this, tr("Delete Skill"),
            tr("Delete skill '%1'?").arg(skill.name()),
            QMessageBox::Yes | QMessageBox::No);

        if (result == QMessageBox::Yes)
        {
            m_documentManager->document().removeSkill(id);
            m_documentManager->markDirty();
        }
    }
}

void SkillsSubView::removePersonFromSkill(const QString& skillId, const QString& personId)
{
    m_documentManager->document().removePersonFromSkill(skillId, personId);
    m_documentManager->markDirty();
}
```

### Step 5: Update CMakeLists.txt

Add to `src/widgets/CMakeLists.txt`:
```
SkillsSubView.cpp
SkillsSubView.h
```

### Step 6: Build and verify

Run: `build.bat`
Expected: Build fails with missing EquipmentSubView, NeedsSubView headers (expected)

---

## Task 3: Create EquipmentSubView

**Files:**
- Create: `src/widgets/EquipmentSubView.h`
- Create: `src/widgets/EquipmentSubView.cpp`
- Modify: `src/widgets/CMakeLists.txt`

### Step 1: Create EquipmentSubView.h

```cpp
#ifndef EQUIPMENTSUBVIEW_H
#define EQUIPMENTSUBVIEW_H

#include <QWidget>
#include <QSet>
#include "MapHighlightProvider.h"

class QTreeWidget;
class QTreeWidgetItem;
class DocumentManager;
class DocumentChange;

class EquipmentSubView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT

public:
    explicit EquipmentSubView(DocumentManager* documentManager, QWidget* parent = nullptr);

    // MapHighlightProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onDocumentChanged(const DocumentChange& change);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);

private:
    void rebuildTree();
    void showAddCategoryDialog();
    void showAddEquipmentDialog(const QString& categoryId);
    void showRenameDialog(QTreeWidgetItem* item);
    void showSelectFamiliesDialog(const QString& equipmentId);
    void deleteItem(QTreeWidgetItem* item);
    void removeFamilyFromEquipment(const QString& equipmentId, const QString& familyId);

    static constexpr int IdRole = Qt::UserRole;
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SecondaryIdRole = Qt::UserRole + 2;
    enum class ItemType { Category, Equipment, Family };

    DocumentManager* m_documentManager;
    QTreeWidget* m_tree;
    QString m_selectedCategoryId;
    QString m_selectedEquipmentId;
    QString m_selectedFamilyId;
};

#endif // EQUIPMENTSUBVIEW_H
```

### Step 2: Create EquipmentSubView.cpp

```cpp
#include "EquipmentSubView.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Equipment.h"
#include "EquipmentCategory.h"
#include "Family.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

EquipmentSubView::EquipmentSubView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
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
            this, &EquipmentSubView::onTreeItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &EquipmentSubView::onContextMenu);
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &EquipmentSubView::onDocumentChanged);

    rebuildTree();
}

void EquipmentSubView::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change);
    rebuildTree();
}

void EquipmentSubView::rebuildTree()
{
    m_tree->clear();

    const Document& doc = m_documentManager->document();
    const QHash<QString, EquipmentCategory>& categories = doc.equipmentCategories();
    const QHash<QString, Equipment>& equipmentList = doc.equipment();

    // Sort categories by sortOrder then name
    QList<EquipmentCategory> sortedCategories = categories.values();
    std::sort(sortedCategories.begin(), sortedCategories.end(),
              [](const EquipmentCategory& a, const EquipmentCategory& b) {
                  if (a.sortOrder() != b.sortOrder())
                  {
                      return a.sortOrder() < b.sortOrder();
                  }
                  return a.name().toLower() < b.name().toLower();
              });

    for (const EquipmentCategory& category : sortedCategories)
    {
        int totalFamilies = 0;
        QList<Equipment> categoryEquipment;
        for (const Equipment& equip : equipmentList)
        {
            if (equip.categoryId() == category.id())
            {
                categoryEquipment.append(equip);
                totalFamilies += equip.familyIds().size();
            }
        }

        std::sort(categoryEquipment.begin(), categoryEquipment.end(),
                  [](const Equipment& a, const Equipment& b) {
                      return a.name().toLower() < b.name().toLower();
                  });

        QTreeWidgetItem* categoryItem = new QTreeWidgetItem();
        categoryItem->setText(0, QString("%1 (%2)").arg(category.name()).arg(totalFamilies));
        categoryItem->setData(0, IdRole, category.id());
        categoryItem->setData(0, TypeRole, static_cast<int>(ItemType::Category));

        if (category.id() == m_selectedCategoryId)
        {
            QFont font = categoryItem->font(0);
            font.setBold(true);
            categoryItem->setFont(0, font);
        }

        for (const Equipment& equip : categoryEquipment)
        {
            QTreeWidgetItem* equipItem = new QTreeWidgetItem(categoryItem);
            equipItem->setText(0, QString("%1 (%2)").arg(equip.name()).arg(equip.familyIds().size()));
            equipItem->setData(0, IdRole, equip.id());
            equipItem->setData(0, TypeRole, static_cast<int>(ItemType::Equipment));

            if (equip.id() == m_selectedEquipmentId)
            {
                QFont font = equipItem->font(0);
                font.setBold(true);
                equipItem->setFont(0, font);
            }

            QList<QString> familyIds = equip.familyIds().values();
            std::sort(familyIds.begin(), familyIds.end(),
                      [&doc](const QString& a, const QString& b) {
                          std::optional<Family> famA = doc.findFamilyById(a);
                          std::optional<Family> famB = doc.findFamilyById(b);
                          QString nameA = famA ? famA->displayName() : a;
                          QString nameB = famB ? famB->displayName() : b;
                          return nameA.toLower() < nameB.toLower();
                      });

            for (const QString& familyId : familyIds)
            {
                std::optional<Family> family = doc.findFamilyById(familyId);
                if (!family)
                {
                    continue;
                }

                QTreeWidgetItem* familyItem = new QTreeWidgetItem(equipItem);
                familyItem->setText(0, family->displayName());
                familyItem->setData(0, IdRole, familyId);
                familyItem->setData(0, TypeRole, static_cast<int>(ItemType::Family));
                familyItem->setData(0, SecondaryIdRole, equip.id());

                if (familyId == m_selectedFamilyId)
                {
                    QFont font = familyItem->font(0);
                    font.setBold(true);
                    familyItem->setFont(0, font);
                }
            }
        }

        m_tree->addTopLevelItem(categoryItem);
        categoryItem->setExpanded(true);
    }
}

void EquipmentSubView::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    m_selectedCategoryId.clear();
    m_selectedEquipmentId.clear();
    m_selectedFamilyId.clear();

    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    switch (type)
    {
    case ItemType::Category:
        m_selectedCategoryId = id;
        break;
    case ItemType::Equipment:
        m_selectedEquipmentId = id;
        break;
    case ItemType::Family:
        m_selectedFamilyId = id;
        m_selectedEquipmentId = item->data(0, SecondaryIdRole).toString();
        break;
    }

    rebuildTree();
    emit highlightChanged();
}

HighlightInfo EquipmentSubView::highlightInfo() const
{
    HighlightInfo info;
    const Document& doc = m_documentManager->document();

    if (!m_selectedFamilyId.isEmpty())
    {
        info.highlightedFamilyIds.insert(m_selectedFamilyId);
    }
    else if (!m_selectedEquipmentId.isEmpty())
    {
        Equipment equip = doc.equipment().value(m_selectedEquipmentId);
        info.highlightedFamilyIds = equip.familyIds();
    }
    else if (!m_selectedCategoryId.isEmpty())
    {
        for (const Equipment& equip : doc.equipment())
        {
            if (equip.categoryId() == m_selectedCategoryId)
            {
                info.highlightedFamilyIds.unite(equip.familyIds());
            }
        }
    }

    return info;
}

QSet<QString> EquipmentSubView::visibleFamilyIds() const
{
    return {};
}

void EquipmentSubView::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    QMenu menu(this);

    if (!item)
    {
        menu.addAction(tr("Add Category..."), this, &EquipmentSubView::showAddCategoryDialog);
    }
    else
    {
        ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
        QString id = item->data(0, IdRole).toString();

        switch (type)
        {
        case ItemType::Category:
        {
            menu.addAction(tr("Rename..."), this, [this, item]() { showRenameDialog(item); });

            const Document& doc = m_documentManager->document();
            EquipmentCategory cat = doc.equipmentCategories().value(id);
            QString addLabel = tr("Add %1 Equipment...").arg(cat.name());
            menu.addAction(addLabel, this, [this, id]() { showAddEquipmentDialog(id); });

            menu.addSeparator();
            menu.addAction(tr("Delete"), this, [this, item]() { deleteItem(item); });
            break;
        }
        case ItemType::Equipment:
        {
            menu.addAction(tr("Select Families..."), this, [this, id]() { showSelectFamiliesDialog(id); });
            menu.addSeparator();
            menu.addAction(tr("Rename..."), this, [this, item]() { showRenameDialog(item); });
            menu.addAction(tr("Delete"), this, [this, item]() { deleteItem(item); });
            break;
        }
        case ItemType::Family:
        {
            QString equipmentId = item->data(0, SecondaryIdRole).toString();

            const Document& doc = m_documentManager->document();
            std::optional<Family> family = doc.findFamilyById(id);
            if (family)
            {
                if (!family->address().isEmpty())
                {
                    QAction* addrAction = menu.addAction(family->address());
                    addrAction->setEnabled(false);
                }
                if (!family->phone().isEmpty())
                {
                    QAction* phoneAction = menu.addAction(family->phone());
                    phoneAction->setEnabled(false);
                }
                if (!family->email().isEmpty())
                {
                    QAction* emailAction = menu.addAction(family->email());
                    emailAction->setEnabled(false);
                }
                if (menu.actions().size() > 0)
                {
                    menu.addSeparator();
                }
            }

            menu.addAction(tr("Remove"), this, [this, equipmentId, id]() { removeFamilyFromEquipment(equipmentId, id); });
            break;
        }
        }
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }
}

void EquipmentSubView::showAddCategoryDialog()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Category"),
                                         tr("Category name:"), QLineEdit::Normal,
                                         QString(), &ok);
    if (ok && !name.isEmpty())
    {
        Document& doc = m_documentManager->document();
        int maxOrder = 0;
        for (const EquipmentCategory& cat : doc.equipmentCategories())
        {
            maxOrder = std::max(maxOrder, cat.sortOrder());
        }

        EquipmentCategory category = EquipmentCategory::create(name, maxOrder + 1);
        doc.addEquipmentCategory(category);
        m_documentManager->markDirty();
    }
}

void EquipmentSubView::showAddEquipmentDialog(const QString& categoryId)
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Equipment"),
                                         tr("Equipment name:"), QLineEdit::Normal,
                                         QString(), &ok);
    if (ok && !name.isEmpty())
    {
        Equipment equip = Equipment::create(name, categoryId);
        m_documentManager->document().addEquipment(equip);
        m_documentManager->markDirty();
    }
}

void EquipmentSubView::showRenameDialog(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    bool ok;
    QString currentName;
    QString title;
    QString label;

    if (type == ItemType::Category)
    {
        const Document& doc = m_documentManager->document();
        EquipmentCategory cat = doc.equipmentCategories().value(id);
        currentName = cat.name();
        title = tr("Rename Category");
        label = tr("Category name:");
    }
    else if (type == ItemType::Equipment)
    {
        const Document& doc = m_documentManager->document();
        Equipment equip = doc.equipment().value(id);
        currentName = equip.name();
        title = tr("Rename Equipment");
        label = tr("Equipment name:");
    }
    else
    {
        return;
    }

    QString newName = QInputDialog::getText(this, title, label,
                                            QLineEdit::Normal, currentName, &ok);
    if (ok && !newName.isEmpty() && newName != currentName)
    {
        Document& doc = m_documentManager->document();
        if (type == ItemType::Category)
        {
            EquipmentCategory cat = doc.equipmentCategories().value(id);
            cat.setName(newName);
            doc.updateEquipmentCategory(cat);
        }
        else
        {
            Equipment equip = doc.equipment().value(id);
            equip.setName(newName);
            doc.updateEquipment(equip);
        }
        m_documentManager->markDirty();
    }
}

void EquipmentSubView::showSelectFamiliesDialog(const QString& equipmentId)
{
    const Document& doc = m_documentManager->document();
    Equipment equip = doc.equipment().value(equipmentId);

    QStringList currentIds = equip.familyIds().values();
    QStringList selectedIds = WardListDialog::selectFamilies(
        m_documentManager, currentIds, this);

    QSet<QString> oldSet(currentIds.begin(), currentIds.end());
    QSet<QString> newSet(selectedIds.begin(), selectedIds.end());

    for (const QString& familyId : oldSet)
    {
        if (!newSet.contains(familyId))
        {
            m_documentManager->document().removeFamilyFromEquipment(equipmentId, familyId);
        }
    }

    for (const QString& familyId : newSet)
    {
        if (!oldSet.contains(familyId))
        {
            m_documentManager->document().addFamilyToEquipment(equipmentId, familyId);
        }
    }

    if (oldSet != newSet)
    {
        m_documentManager->markDirty();
    }
}

void EquipmentSubView::deleteItem(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    if (type == ItemType::Category)
    {
        const Document& doc = m_documentManager->document();
        EquipmentCategory cat = doc.equipmentCategories().value(id);

        QMessageBox::StandardButton result = QMessageBox::question(this, tr("Delete Category"),
            tr("Delete category '%1' and all its equipment?").arg(cat.name()),
            QMessageBox::Yes | QMessageBox::No);

        if (result == QMessageBox::Yes)
        {
            Document& docMut = m_documentManager->document();
            QList<QString> toDelete;
            for (const Equipment& equip : docMut.equipment())
            {
                if (equip.categoryId() == id)
                {
                    toDelete.append(equip.id());
                }
            }
            for (const QString& equipId : toDelete)
            {
                docMut.removeEquipment(equipId);
            }
            docMut.removeEquipmentCategory(id);
            m_documentManager->markDirty();
        }
    }
    else if (type == ItemType::Equipment)
    {
        const Document& doc = m_documentManager->document();
        Equipment equip = doc.equipment().value(id);

        QMessageBox::StandardButton result = QMessageBox::question(this, tr("Delete Equipment"),
            tr("Delete equipment '%1'?").arg(equip.name()),
            QMessageBox::Yes | QMessageBox::No);

        if (result == QMessageBox::Yes)
        {
            m_documentManager->document().removeEquipment(id);
            m_documentManager->markDirty();
        }
    }
}

void EquipmentSubView::removeFamilyFromEquipment(const QString& equipmentId, const QString& familyId)
{
    m_documentManager->document().removeFamilyFromEquipment(equipmentId, familyId);
    m_documentManager->markDirty();
}
```

### Step 3: Update CMakeLists.txt

Add to `src/widgets/CMakeLists.txt`:
```
EquipmentSubView.cpp
EquipmentSubView.h
```

### Step 4: Build and verify

Run: `build.bat`
Expected: Build fails with missing NeedsSubView header (expected)

---

## Task 4: Create NeedsSubView

**Files:**
- Create: `src/widgets/NeedsSubView.h`
- Create: `src/widgets/NeedsSubView.cpp`
- Modify: `src/widgets/CMakeLists.txt`

### Step 1: Create NeedsSubView.h

```cpp
#ifndef NEEDSSUBVIEW_H
#define NEEDSSUBVIEW_H

#include <QWidget>
#include <QSet>
#include "MapHighlightProvider.h"

class QTreeWidget;
class QTreeWidgetItem;
class DocumentManager;
class DocumentChange;

class NeedsSubView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT

public:
    explicit NeedsSubView(DocumentManager* documentManager, QWidget* parent = nullptr);

    // MapHighlightProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onDocumentChanged(const DocumentChange& change);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);

private:
    void rebuildTree();
    void showAddNeedDialog();
    void showEditNeedDialog(QTreeWidgetItem* item);
    void deleteNeed(QTreeWidgetItem* item);

    static constexpr int PersonIdRole = Qt::UserRole;
    static constexpr int FamilyIdRole = Qt::UserRole + 1;

    DocumentManager* m_documentManager;
    QTreeWidget* m_tree;
    QString m_selectedPersonId;  // Empty if no person selected
    QString m_selectedFamilyId;  // Empty if no family selected
};

#endif // NEEDSSUBVIEW_H
```

### Step 2: Create NeedsSubView.cpp

```cpp
#include "NeedsSubView.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "DocumentChange.h"
#include "SpecialNeed.h"
#include "Person.h"
#include "Family.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>

NeedsSubView::NeedsSubView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);  // Flat list
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

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
            this, &NeedsSubView::onTreeItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &NeedsSubView::onContextMenu);
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &NeedsSubView::onDocumentChanged);

    rebuildTree();
}

void NeedsSubView::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change);
    rebuildTree();
}

void NeedsSubView::rebuildTree()
{
    m_tree->clear();

    const Document& doc = m_documentManager->document();
    const QList<SpecialNeed>& needs = doc.specialNeeds();

    // Build list with display names for sorting
    struct NeedDisplay
    {
        SpecialNeed need;
        QString displayName;
    };
    QList<NeedDisplay> sortedNeeds;

    for (const SpecialNeed& need : needs)
    {
        QString name;
        if (need.personId)
        {
            std::optional<Person> person = doc.findPersonById(*need.personId);
            name = person ? person->displayName() : *need.personId;
        }
        else if (need.familyId)
        {
            std::optional<Family> family = doc.findFamilyById(*need.familyId);
            name = family ? family->displayName() : *need.familyId;
        }
        sortedNeeds.append({need, name});
    }

    std::sort(sortedNeeds.begin(), sortedNeeds.end(),
              [](const NeedDisplay& a, const NeedDisplay& b) {
                  return a.displayName.toLower() < b.displayName.toLower();
              });

    for (const NeedDisplay& nd : sortedNeeds)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        QString text = QString("%1 - %2").arg(nd.displayName, nd.need.note);
        item->setText(0, text);
        item->setData(0, PersonIdRole, nd.need.personId.value_or(QString()));
        item->setData(0, FamilyIdRole, nd.need.familyId.value_or(QString()));

        bool isSelected = false;
        if (!m_selectedPersonId.isEmpty() && nd.need.personId
            && m_selectedPersonId == *nd.need.personId)
        {
            isSelected = true;
        }
        else if (!m_selectedFamilyId.isEmpty() && nd.need.familyId
                 && m_selectedFamilyId == *nd.need.familyId)
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

void NeedsSubView::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    m_selectedPersonId.clear();
    m_selectedFamilyId.clear();

    QString personId = item->data(0, PersonIdRole).toString();
    QString familyId = item->data(0, FamilyIdRole).toString();

    if (!personId.isEmpty())
    {
        m_selectedPersonId = personId;
    }
    if (!familyId.isEmpty())
    {
        m_selectedFamilyId = familyId;
    }

    rebuildTree();
    emit highlightChanged();
}

HighlightInfo NeedsSubView::highlightInfo() const
{
    HighlightInfo info;
    const Document& doc = m_documentManager->document();

    if (!m_selectedPersonId.isEmpty())
    {
        QString familyId = doc.familyIdForPerson(m_selectedPersonId);
        if (!familyId.isEmpty())
        {
            info.highlightedFamilyIds.insert(familyId);
        }
    }
    else if (!m_selectedFamilyId.isEmpty())
    {
        info.highlightedFamilyIds.insert(m_selectedFamilyId);
    }

    return info;
}

QSet<QString> NeedsSubView::visibleFamilyIds() const
{
    return {};
}

void NeedsSubView::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    QMenu menu(this);

    if (!item)
    {
        menu.addAction(tr("Add Special Need..."), this, &NeedsSubView::showAddNeedDialog);
    }
    else
    {
        QString personId = item->data(0, PersonIdRole).toString();
        QString familyId = item->data(0, FamilyIdRole).toString();

        // Show contact info
        const Document& doc = m_documentManager->document();
        if (!personId.isEmpty())
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (person)
            {
                QString famId = doc.familyIdForPerson(personId);
                std::optional<Family> family = doc.findFamilyById(famId);

                if (family && !family->address().isEmpty())
                {
                    QAction* addrAction = menu.addAction(family->address());
                    addrAction->setEnabled(false);
                }
                if (!person->phone().isEmpty())
                {
                    QAction* phoneAction = menu.addAction(person->phone());
                    phoneAction->setEnabled(false);
                }
                if (!person->email().isEmpty())
                {
                    QAction* emailAction = menu.addAction(person->email());
                    emailAction->setEnabled(false);
                }
            }
        }
        else if (!familyId.isEmpty())
        {
            std::optional<Family> family = doc.findFamilyById(familyId);
            if (family)
            {
                if (!family->address().isEmpty())
                {
                    QAction* addrAction = menu.addAction(family->address());
                    addrAction->setEnabled(false);
                }
                if (!family->phone().isEmpty())
                {
                    QAction* phoneAction = menu.addAction(family->phone());
                    phoneAction->setEnabled(false);
                }
                if (!family->email().isEmpty())
                {
                    QAction* emailAction = menu.addAction(family->email());
                    emailAction->setEnabled(false);
                }
            }
        }

        if (menu.actions().size() > 0)
        {
            menu.addSeparator();
        }

        menu.addAction(tr("Edit..."), this, [this, item]() { showEditNeedDialog(item); });
        menu.addAction(tr("Delete"), this, [this, item]() { deleteNeed(item); });
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }
}

void NeedsSubView::showAddNeedDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add Special Need"));

    QFormLayout* formLayout = new QFormLayout(&dialog);

    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItem(tr("Person"), "person");
    typeCombo->addItem(tr("Family"), "family");
    formLayout->addRow(tr("Type:"), typeCombo);

    QPushButton* selectBtn = new QPushButton(tr("Select..."));
    QString selectedId;
    formLayout->addRow(tr("Who:"), selectBtn);

    QLineEdit* noteEdit = new QLineEdit();
    formLayout->addRow(tr("Note:"), noteEdit);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    formLayout->addRow(buttonBox);

    connect(selectBtn, &QPushButton::clicked, this, [&]() {
        if (typeCombo->currentData().toString() == "person")
        {
            selectedId = WardListDialog::selectPerson(m_documentManager, QString(), &dialog);
            if (!selectedId.isEmpty())
            {
                std::optional<Person> person = m_documentManager->document().findPersonById(selectedId);
                selectBtn->setText(person ? person->displayName() : selectedId);
            }
        }
        else
        {
            selectedId = WardListDialog::selectFamily(m_documentManager, QString(), &dialog);
            if (!selectedId.isEmpty())
            {
                std::optional<Family> family = m_documentManager->document().findFamilyById(selectedId);
                selectBtn->setText(family ? family->displayName() : selectedId);
            }
        }
    });

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted && !selectedId.isEmpty() && !noteEdit->text().isEmpty())
    {
        SpecialNeed need;
        need.note = noteEdit->text();
        if (typeCombo->currentData().toString() == "person")
        {
            need.personId = selectedId;
        }
        else
        {
            need.familyId = selectedId;
        }

        m_documentManager->document().setSpecialNeed(need);
        m_documentManager->markDirty();
    }
}

void NeedsSubView::showEditNeedDialog(QTreeWidgetItem* item)
{
    QString personId = item->data(0, PersonIdRole).toString();
    QString familyId = item->data(0, FamilyIdRole).toString();

    std::optional<QString> personIdOpt = personId.isEmpty() ? std::nullopt : std::optional(personId);
    std::optional<QString> familyIdOpt = familyId.isEmpty() ? std::nullopt : std::optional(familyId);

    std::optional<SpecialNeed> existingNeed = m_documentManager->document().findSpecialNeed(personIdOpt, familyIdOpt);
    if (!existingNeed)
    {
        return;
    }

    bool ok;
    QString newNote = QInputDialog::getText(this, tr("Edit Special Need"),
                                            tr("Note:"), QLineEdit::Normal,
                                            existingNeed->note, &ok);
    if (ok && !newNote.isEmpty())
    {
        SpecialNeed updated = *existingNeed;
        updated.note = newNote;
        m_documentManager->document().setSpecialNeed(updated);
        m_documentManager->markDirty();
    }
}

void NeedsSubView::deleteNeed(QTreeWidgetItem* item)
{
    QString personId = item->data(0, PersonIdRole).toString();
    QString familyId = item->data(0, FamilyIdRole).toString();

    std::optional<QString> personIdOpt = personId.isEmpty() ? std::nullopt : std::optional(personId);
    std::optional<QString> familyIdOpt = familyId.isEmpty() ? std::nullopt : std::optional(familyId);

    QMessageBox::StandardButton result = QMessageBox::question(this, tr("Delete Special Need"),
        tr("Delete this special need?"),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes)
    {
        m_documentManager->document().clearSpecialNeed(personIdOpt, familyIdOpt);
        m_documentManager->markDirty();
    }
}
```

### Step 3: Update CMakeLists.txt

Add to `src/widgets/CMakeLists.txt`:
```
NeedsSubView.cpp
NeedsSubView.h
```

### Step 4: Build and verify

Run: `build.bat`
Expected: Build succeeds (all sub-views now exist)

---

## Task 5: Integrate EmergencyView into MainWindow

**Files:**
- Modify: `src/widgets/MainWindow.h`
- Modify: `src/widgets/MainWindow.cpp`

### Step 1: Add EmergencyView include and member

In MainWindow.h, add forward declaration:
```cpp
class EmergencyView;
```

Add member variable in private section:
```cpp
EmergencyView* m_emergencyView;
```

### Step 2: Add include in MainWindow.cpp

```cpp
#include "EmergencyView.h"
```

### Step 3: Create EmergencyView and add tab

In MainWindow.cpp, after creating m_ministeringView and adding its tab, add:

```cpp
m_emergencyView = new EmergencyView(m_documentManager);
m_sidebarTabs->addTab(m_emergencyView, tr("Emergency"));
```

### Step 4: Connect highlightChanged signal

After the existing connect for m_ministeringView, add:

```cpp
connect(m_emergencyView, &EmergencyView::highlightChanged,
        m_mapWidget, &MapWidget::updateHighlights);
```

### Step 5: Build and verify

Run: `build.bat`
Expected: Build succeeds

### Step 6: Manual test

1. Run the application
2. Verify "Emergency" tab appears in sidebar
3. Click on Emergency tab - should show sub-tabs (Teams, Skills, Equipment, Needs)
4. Verify Skills, Equipment, Needs tabs are selectable
5. Right-click on empty area in Skills - should show "Add Category..." option

---

## Task 6: Commit checkpoint

### Step 1: Stage and commit

```bash
git add -A
git commit -m "feat(emergency): add Emergency tab with Skills, Equipment, Needs sub-views

Implements the Emergency sidebar tab with:
- EmergencyView container with QTabWidget for sub-tabs
- SkillsSubView with 3-level tree (Category -> Skill -> Person)
- EquipmentSubView with 3-level tree (Category -> Equipment -> Family)
- NeedsSubView with flat list of special needs
- MapHighlightProvider integration for all sub-views
- Context menus for add/edit/delete operations
- WardListDialog integration for selecting people/families

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Summary

This plan creates the core Emergency tab functionality:

1. **EmergencyView** - Container that delegates MapHighlightProvider to active sub-view
2. **SkillsSubView** - Category -> Skill -> Person hierarchy with person selection
3. **EquipmentSubView** - Category -> Equipment -> Family hierarchy with family selection
4. **NeedsSubView** - Flat list of special needs with person/family association
5. **MainWindow integration** - Tab and signal connections

Future enhancements (not in this plan):
- Emergency menu bar integration
- Teams sub-tab implementation
- Contact details as expandable children
