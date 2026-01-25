#include "EquipmentSubView.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Equipment.h"
#include "EquipmentCategory.h"
#include "Family.h"
#include "EquipmentCommands.h"

#include <QTreeWidget>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

EquipmentSubView::EquipmentSubView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
    , m_tree(nullptr)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked,
            this, &EquipmentSubView::onTreeItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &EquipmentSubView::onContextMenu);
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &EquipmentSubView::onDocumentChanged);

    rebuildTree();
}

void EquipmentSubView::rebuildTree()
{
    m_tree->clear();

    const Document& doc = m_documentManager->document();
    const auto& categories = doc.equipmentCategories();
    const auto& allEquipment = doc.equipment();

    // Sort categories by sortOrder then name
    QList<EquipmentCategory> sortedCategories = categories.values();
    std::sort(sortedCategories.begin(), sortedCategories.end(),
              [](const EquipmentCategory& a, const EquipmentCategory& b)
              {
                  if (a.sortOrder() != b.sortOrder())
                  {
                      return a.sortOrder() < b.sortOrder();
                  }
                  return a.name().toLower() < b.name().toLower();
              });

    for (const EquipmentCategory& category : sortedCategories)
    {
        // Get equipment in this category
        QList<Equipment> categoryEquipment;
        for (const auto& equip : allEquipment)
        {
            if (equip.categoryId() == category.id())
            {
                categoryEquipment.append(equip);
            }
        }

        // Sort equipment by name
        std::sort(categoryEquipment.begin(), categoryEquipment.end(),
                  [](const Equipment& a, const Equipment& b)
                  {
                      return a.name().toLower() < b.name().toLower();
                  });

        // Count total families in category
        int totalFamilies = 0;
        for (const Equipment& equip : categoryEquipment)
        {
            totalFamilies += equip.familyIds().size();
        }

        // Create category item
        QTreeWidgetItem* categoryItem = new QTreeWidgetItem();
        categoryItem->setText(0, QString("%1 (%2)").arg(category.name()).arg(totalFamilies));
        categoryItem->setData(0, IdRole, category.id());
        categoryItem->setData(0, TypeRole, static_cast<int>(ItemType::Category));

        // Bold if selected
        if (m_selectedCategoryId == category.id())
        {
            QFont font = categoryItem->font(0);
            font.setBold(true);
            categoryItem->setFont(0, font);
        }

        // Add equipment under this category
        for (const Equipment& equip : categoryEquipment)
        {
            QTreeWidgetItem* equipItem = new QTreeWidgetItem(categoryItem);
            equipItem->setText(0, QString("%1 (%2)").arg(equip.name()).arg(equip.familyIds().size()));
            equipItem->setData(0, IdRole, equip.id());
            equipItem->setData(0, TypeRole, static_cast<int>(ItemType::Equipment));

            // Bold if selected
            if (m_selectedEquipmentId == equip.id())
            {
                QFont font = equipItem->font(0);
                font.setBold(true);
                equipItem->setFont(0, font);
            }

            // Get families with this equipment, sorted by display name
            QList<QPair<QString, QString>> families;  // (familyId, displayName)
            for (const QString& familyId : equip.familyIds())
            {
                auto familyIt = doc.families().constFind(familyId);
                if (familyIt != doc.families().constEnd())
                {
                    families.append({familyId, familyIt->displayName()});
                }
            }
            std::sort(families.begin(), families.end(),
                      [](const auto& a, const auto& b)
                      {
                          return a.second.toLower() < b.second.toLower();
                      });

            // Add family items
            for (const auto& [familyId, displayName] : families)
            {
                QTreeWidgetItem* familyItem = new QTreeWidgetItem(equipItem);
                familyItem->setText(0, displayName);
                familyItem->setData(0, IdRole, familyId);
                familyItem->setData(0, TypeRole, static_cast<int>(ItemType::Family));
                familyItem->setData(0, SecondaryIdRole, equip.id());

                // Bold if selected
                if (m_selectedFamilyId == familyId)
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

void EquipmentSubView::onTreeItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    // Clear all selections
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

void EquipmentSubView::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);

    QMenu menu;

    if (!item)
    {
        // No item - show add category option
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
                const Document& doc = m_documentManager->document();
                if (!doc.equipmentCategories().contains(id))
                {
                    break;
                }
                const EquipmentCategory& category = doc.equipmentCategories().value(id);
                QString categoryName = category.name();

                menu.addAction(tr("Rename..."), this, [this, item]()
                {
                    showRenameDialog(item);
                });
                menu.addAction(tr("Add %1 Equipment...").arg(categoryName), this, [this, id]()
                {
                    showAddEquipmentDialog(id);
                });
                menu.addSeparator();
                menu.addAction(tr("Delete"), this, [this, item]()
                {
                    deleteItem(item);
                });
            }
            break;

        case ItemType::Equipment:
            {
                menu.addAction(tr("Select Families..."), this, [this, id]()
                {
                    showSelectFamiliesDialog(id);
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

        case ItemType::Family:
            {
                // Show contact info (disabled) if available
                const Document& doc = m_documentManager->document();
                auto familyIt = doc.families().constFind(id);
                if (familyIt != doc.families().constEnd())
                {
                    const Family& family = *familyIt;

                    // Show family phone/email
                    const Phone& phone = family.displayPhone();
                    if (!phone.isEmpty())
                    {
                        QAction* phoneAction = menu.addAction(phone);
                        phoneAction->setEnabled(false);
                    }
                    const QString& email = family.displayEmail();
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

                QString equipmentId = item->data(0, SecondaryIdRole).toString();
                menu.addAction(tr("Remove"), this, [this, equipmentId, id]()
                {
                    removeFamilyFromEquipment(equipmentId, id);
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

void EquipmentSubView::onDocumentChanged(const DocumentChange& change)
{
    switch (change.scope)
    {
    case ChangeScope::Full:
    case ChangeScope::EquipmentCategory:
    case ChangeScope::Equipment:
    case ChangeScope::Family:  // Family names
        validateSelections();
        rebuildTree();
        emit highlightChanged();
        break;
    default:
        break;
    }
}

void EquipmentSubView::validateSelections()
{
    const Document& doc = m_documentManager->document();

    // Clear category selection if it no longer exists
    if (!m_selectedCategoryId.isEmpty()
        && !doc.equipmentCategories().contains(m_selectedCategoryId))
    {
        m_selectedCategoryId.clear();
    }

    // Clear equipment selection if it no longer exists
    if (!m_selectedEquipmentId.isEmpty() && !doc.findEquipmentById(m_selectedEquipmentId))
    {
        m_selectedEquipmentId.clear();
    }

    // Clear family selection if they no longer exist or are no longer in the equipment
    if (!m_selectedFamilyId.isEmpty())
    {
        bool valid = false;
        if (!m_selectedEquipmentId.isEmpty())
        {
            auto equipOpt = doc.findEquipmentById(m_selectedEquipmentId);
            if (equipOpt && equipOpt->familyIds().contains(m_selectedFamilyId))
            {
                valid = true;
            }
        }
        if (!valid)
        {
            m_selectedFamilyId.clear();
        }
    }
}

HighlightInfo EquipmentSubView::highlightInfo() const
{
    HighlightInfo info;
    const Document& doc = m_documentManager->document();

    if (!m_selectedFamilyId.isEmpty())
    {
        // Single family selected - highlight that family
        info.highlightedFamilyIds.insert(m_selectedFamilyId);
    }
    else if (!m_selectedEquipmentId.isEmpty())
    {
        // Equipment selected - highlight all families with this equipment
        auto equipOpt = doc.findEquipmentById(m_selectedEquipmentId);
        if (equipOpt)
        {
            for (const QString& familyId : equipOpt->familyIds())
            {
                info.highlightedFamilyIds.insert(familyId);
            }
        }
    }
    else if (!m_selectedCategoryId.isEmpty())
    {
        // Category selected - highlight all families in all equipment in this category
        const auto& allEquipment = doc.equipment();
        for (const auto& equip : allEquipment)
        {
            if (equip.categoryId() == m_selectedCategoryId)
            {
                for (const QString& familyId : equip.familyIds())
                {
                    info.highlightedFamilyIds.insert(familyId);
                }
            }
        }
    }

    return info;
}

QSet<QString> EquipmentSubView::visibleFamilyIds() const
{
    // Show all families
    return {};
}

void EquipmentSubView::showAddCategoryDialog()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Category"),
                                          tr("Category name:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty())
    {
        const Document& doc = m_documentManager->document();
        const auto& categories = doc.equipmentCategories();

        // Find max sort order
        int maxOrder = 0;
        for (const auto& category : categories)
        {
            if (category.sortOrder() > maxOrder)
            {
                maxOrder = category.sortOrder();
            }
        }

        EquipmentCategory newCategory = EquipmentCategory::create(name, maxOrder + 1);
        m_documentManager->executeCommand(std::make_unique<AddEquipmentCategoryCommand>(newCategory));
    }
}

void EquipmentSubView::showAddEquipmentDialog(const QString& categoryId)
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Equipment"),
                                          tr("Equipment name:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty())
    {
        Equipment newEquipment = Equipment::create(name, categoryId);
        m_documentManager->executeCommand(std::make_unique<AddEquipmentCommand>(newEquipment));
    }
}

void EquipmentSubView::showRenameDialog(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    const Document& doc = m_documentManager->document();

    if (type == ItemType::Category)
    {
        if (!doc.equipmentCategories().contains(id))
        {
            return;
        }
        const EquipmentCategory& category = doc.equipmentCategories().value(id);
        bool ok;
        QString name = QInputDialog::getText(this, tr("Rename Category"),
                                              tr("Category name:"),
                                              QLineEdit::Normal, category.name(), &ok);
        if (ok && !name.isEmpty() && name != category.name())
        {
            EquipmentCategory updated = category;
            updated.setName(name);
            m_documentManager->executeCommand(
                std::make_unique<UpdateEquipmentCategoryCommand>(category, updated));
        }
    }
    else if (type == ItemType::Equipment)
    {
        auto equipOpt = doc.findEquipmentById(id);
        if (equipOpt)
        {
            bool ok;
            QString name = QInputDialog::getText(this, tr("Rename Equipment"),
                                                  tr("Equipment name:"),
                                                  QLineEdit::Normal, equipOpt->name(), &ok);
            if (ok && !name.isEmpty() && name != equipOpt->name())
            {
                Equipment updated = *equipOpt;
                updated.setName(name);
                m_documentManager->executeCommand(
                    std::make_unique<UpdateEquipmentCommand>(*equipOpt, updated));
            }
        }
    }
}

void EquipmentSubView::showSelectFamiliesDialog(const QString& equipmentId)
{
    const Document& doc = m_documentManager->document();
    auto equipOpt = doc.findEquipmentById(equipmentId);
    if (!equipOpt)
    {
        return;
    }

    // Get current family IDs as a list
    QStringList currentIds = equipOpt->familyIds().values();

    // Show dialog to select families
    QStringList selectedIds = WardListDialog::selectFamilies(m_documentManager, currentIds, this);

    // Check if selection changed
    QSet<QString> newSet(selectedIds.begin(), selectedIds.end());
    if (newSet == equipOpt->familyIds())
    {
        return;  // No change
    }

    // Update equipment with new family IDs (single undo operation)
    Equipment updated = *equipOpt;
    updated.setFamilyIds(newSet);
    m_documentManager->executeCommand(
        std::make_unique<UpdateEquipmentCommand>(*equipOpt, updated));
}

void EquipmentSubView::deleteItem(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    const Document& doc = m_documentManager->document();

    if (type == ItemType::Category)
    {
        if (!doc.equipmentCategories().contains(id))
        {
            return;
        }
        const EquipmentCategory& category = doc.equipmentCategories().value(id);

        // Count equipment in this category
        int equipCount = 0;
        for (const auto& equip : doc.equipment())
        {
            if (equip.categoryId() == id)
            {
                equipCount++;
            }
        }

        QString message = equipCount > 0
            ? tr("Delete category \"%1\" and its %2 equipment item(s)?").arg(category.name()).arg(equipCount)
            : tr("Delete category \"%1\"?").arg(category.name());

        if (QMessageBox::question(this, tr("Delete Category"), message) == QMessageBox::Yes)
        {
            // Delete equipment in this category first
            for (const auto& equip : doc.equipment())
            {
                if (equip.categoryId() == id)
                {
                    m_documentManager->executeCommand(
                        std::make_unique<DeleteEquipmentCommand>(equip));
                }
            }

            // Then delete the category
            m_documentManager->executeCommand(
                std::make_unique<DeleteEquipmentCategoryCommand>(category));
        }
    }
    else if (type == ItemType::Equipment)
    {
        auto equipOpt = doc.findEquipmentById(id);
        if (equipOpt)
        {
            QString message = tr("Delete equipment \"%1\"?").arg(equipOpt->name());
            if (QMessageBox::question(this, tr("Delete Equipment"), message) == QMessageBox::Yes)
            {
                m_documentManager->executeCommand(
                    std::make_unique<DeleteEquipmentCommand>(*equipOpt));
            }
        }
    }
}

void EquipmentSubView::removeFamilyFromEquipment(const QString& equipmentId, const QString& familyId)
{
    m_documentManager->executeCommand(
        std::make_unique<UnassignEquipmentFromFamilyCommand>(equipmentId, familyId));
}
