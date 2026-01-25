#include "MinisteringView.h"
#include "DocumentManager.h"
#include "Document.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "Family.h"
#include "Person.h"

#include <utility>

#include <QVBoxLayout>
#include <QTabBar>
#include <QTreeWidget>
#include <QHeaderView>

MinisteringView::MinisteringView(DocumentManager* docManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(docManager)
{
    setupUi();

    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &MinisteringView::onDocumentChanged);

    // Initial population - rebuild all 4 trees
    rebuildTreeImpl(m_eqTree, true);
    rebuildTreeImpl(m_rsTree, false);
    rebuildUnassignedTreeImpl(m_eqUnassignedTree, true);
    rebuildUnassignedTreeImpl(m_rsUnassignedTree, false);
    updateUnassignedVisibility();
}

void MinisteringView::setupUi()
{
    setMinimumWidth(250);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // EQ/RS tabs
    m_orgTabs = new QTabBar();
    m_orgTabs->addTab(tr("Elders Quorum"));
    m_orgTabs->addTab(tr("Relief Society"));
    layout->addWidget(m_orgTabs);

    // Helper to create tree widgets with common configuration
    auto createTree = [](bool isUnassigned) {
        auto* tree = new QTreeWidget();
        tree->setHeaderHidden(true);
        tree->setRootIsDecorated(true);
        tree->setSelectionMode(QAbstractItemView::NoSelection);
        tree->setIndentation(16);
        if (isUnassigned)
        {
            tree->setFixedHeight(38);  // Single row height when collapsed
        }
        return tree;
    };

    // Create all 4 trees
    m_eqUnassignedTree = createTree(true);
    m_rsUnassignedTree = createTree(true);
    m_eqTree = createTree(false);
    m_rsTree = createTree(false);

    // RS trees start hidden
    m_rsTree->setVisible(false);
    m_rsUnassignedTree->setVisible(false);

    // Add to layout - unassigned trees first, then main trees
    layout->addWidget(m_eqUnassignedTree);
    layout->addWidget(m_rsUnassignedTree);
    layout->addWidget(m_eqTree, 1);
    layout->addWidget(m_rsTree, 1);

    // Connections - tab switching
    connect(m_orgTabs, &QTabBar::currentChanged,
            this, &MinisteringView::onOrgToggled);

    // Main tree connections
    connect(m_eqTree, &QTreeWidget::itemClicked,
            this, &MinisteringView::onTreeItemClicked);
    connect(m_rsTree, &QTreeWidget::itemClicked,
            this, &MinisteringView::onTreeItemClicked);
    connect(m_eqTree, &QTreeWidget::itemExpanded,
            this, &MinisteringView::onTreeItemExpanded);
    connect(m_rsTree, &QTreeWidget::itemExpanded,
            this, &MinisteringView::onTreeItemExpanded);

    // Unassigned tree connections
    connect(m_eqUnassignedTree, &QTreeWidget::itemClicked,
            this, &MinisteringView::onTreeItemClicked);
    connect(m_rsUnassignedTree, &QTreeWidget::itemClicked,
            this, &MinisteringView::onTreeItemClicked);
    connect(m_eqUnassignedTree, &QTreeWidget::itemExpanded,
            this, &MinisteringView::onUnassignedTreeItemExpanded);
    connect(m_rsUnassignedTree, &QTreeWidget::itemExpanded,
            this, &MinisteringView::onUnassignedTreeItemExpanded);
    connect(m_eqUnassignedTree, &QTreeWidget::itemCollapsed,
            this, &MinisteringView::onUnassignedTreeItemCollapsed);
    connect(m_rsUnassignedTree, &QTreeWidget::itemCollapsed,
            this, &MinisteringView::onUnassignedTreeItemCollapsed);
}

void MinisteringView::onOrgToggled(int id)
{
    m_isEQ = (id == 0);

    // Show/hide trees - no rebuild needed
    m_eqTree->setVisible(m_isEQ);
    m_rsTree->setVisible(!m_isEQ);
    updateUnassignedVisibility();

    emit highlightChanged();
}

void MinisteringView::onTreeItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());

    bool isEQ = (tree == m_eqTree || tree == m_eqUnassignedTree);
    bool isUnassigned = (tree == m_eqUnassignedTree || tree == m_rsUnassignedTree);

    handleTreeItemClicked(item, isEQ, isUnassigned);
}

void MinisteringView::onUnassignedTreeItemExpanded(QTreeWidgetItem* item)
{
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    // Resize tree when header is expanded
    if (type == ItemType::UnassignedHeader)
    {
        tree->setFixedHeight(200);
    }
    // Populate contact info on expand for families/sisters
    else if (type == ItemType::MinisteredFamily || type == ItemType::MinisteredSister)
    {
        if (item->childCount() == 0)
        {
            populateContactInfo(item);
        }
    }
}

void MinisteringView::onUnassignedTreeItemCollapsed(QTreeWidgetItem* item)
{
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    // Shrink tree when header is collapsed
    if (type == ItemType::UnassignedHeader)
    {
        tree->setFixedHeight(38);
    }
}

void MinisteringView::handleTreeItemClicked(QTreeWidgetItem* item, bool isEQ, bool isUnassigned)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString itemId = item->data(0, IdRole).toString();

    // Contact details are not interactive
    if (type == ItemType::ContactDetail)
    {
        return;
    }

    // Get references to the appropriate selection state
    QString& selectedId = isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType& selectedType = isEQ ? m_eqSelectedType : m_rsSelectedType;
    QTreeWidgetItem*& selectedItem = isEQ ? m_eqSelectedItem : m_rsSelectedItem;

    // Build the selection ID based on type
    QString selectionId;
    if (type == ItemType::UnassignedHeader)
    {
        selectionId = "unassigned";
    }
    else if (type == ItemType::SectionHeader)
    {
        // Section headers need composite ID: "{compId}:ministers" or "{compId}:ministered"
        QTreeWidgetItem* parent = item->parent();
        if (parent)
        {
            QString compId = parent->data(0, IdRole).toString();
            QString sectionName = item->text(0);
            if (sectionName == tr("Ministers"))
            {
                selectionId = compId + ":ministers";
            }
            else
            {
                selectionId = compId + ":ministered";
            }
        }
    }
    else
    {
        selectionId = itemId;
    }

    // Unbold previously selected item
    if (selectedItem)
    {
        QFont font = selectedItem->font(0);
        font.setBold(false);
        selectedItem->setFont(0, font);
    }

    // Toggle selection: if already selected, deselect; otherwise select
    if (selectedId == selectionId && selectedType == type)
    {
        clearSelection(isEQ);
        selectedItem = nullptr;
    }
    else
    {
        selectedId = selectionId;
        selectedType = type;
        selectedItem = item;

        // Bold newly selected item
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
    }

    emit highlightChanged();
}

void MinisteringView::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change)

    // Clear item pointers (invalidated by rebuild)
    m_eqSelectedItem = nullptr;
    m_rsSelectedItem = nullptr;

    // Rebuild all 4 trees
    rebuildTreeImpl(m_eqTree, true);
    rebuildTreeImpl(m_rsTree, false);
    rebuildUnassignedTreeImpl(m_eqUnassignedTree, true);
    rebuildUnassignedTreeImpl(m_rsUnassignedTree, false);

    updateUnassignedVisibility();
    emit highlightChanged();
}

void MinisteringView::onTreeItemExpanded(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    if (type == ItemType::Companionship)
    {
        // Expand section headers (Ministers, Families/Sisters) when companionship is expanded
        for (int i = 0; i < item->childCount(); ++i)
        {
            QTreeWidgetItem* child = item->child(i);
            ItemType childType = static_cast<ItemType>(child->data(0, TypeRole).toInt());
            if (childType == ItemType::SectionHeader)
            {
                child->setExpanded(true);
            }
        }
    }
    else if (type == ItemType::Minister
             || type == ItemType::MinisteredFamily
             || type == ItemType::MinisteredSister)
    {
        // Only populate contact info if not already done
        if (item->childCount() == 0)
        {
            populateContactInfo(item);
        }
    }
}

void MinisteringView::populateContactInfo(QTreeWidgetItem* item)
{
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());
    QString id = item->data(0, IdRole).toString();

    const Document& doc = m_documentManager->document();

    if (type == ItemType::Minister || type == ItemType::MinisteredSister)
    {
        // Person contact info
        std::optional<Person> person = doc.findPersonById(id);
        if (!person)
        {
            return;
        }

        // Phone
        if (!person->phone().isEmpty())
        {
            QTreeWidgetItem* phoneItem = new QTreeWidgetItem(item);
            phoneItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x9e ") + person->phone());
            phoneItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
            phoneItem->setFlags(phoneItem->flags() & ~Qt::ItemIsSelectable);
        }

        // Alt phone
        if (!person->altPhone().isEmpty())
        {
            QTreeWidgetItem* altPhoneItem = new QTreeWidgetItem(item);
            altPhoneItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x9e ") + person->altPhone() + tr(" (alt)"));
            altPhoneItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
            altPhoneItem->setFlags(altPhoneItem->flags() & ~Qt::ItemIsSelectable);
        }

        // Email
        if (!person->email().isEmpty())
        {
            QTreeWidgetItem* emailItem = new QTreeWidgetItem(item);
            emailItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\xa7 ") + person->email());
            emailItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
            emailItem->setFlags(emailItem->flags() & ~Qt::ItemIsSelectable);
        }

        // Address (from family)
        QString familyId = doc.familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            const auto& families = doc.families();
            if (families.contains(familyId))
            {
                const Family& family = families[familyId];
                if (!family.address().isEmpty())
                {
                    QTreeWidgetItem* addrItem = new QTreeWidgetItem(item);
                    addrItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x8d ") + family.address().full());
                    addrItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
                    addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsSelectable);
                }
            }
        }
    }
    else if (type == ItemType::MinisteredFamily)
    {
        // Family contact info
        const auto& families = doc.families();
        if (!families.contains(id))
        {
            return;
        }

        const Family& family = families[id];

        // Find head of household for phone
        for (const Person& member : family.members())
        {
            if (member.isParent() && !member.phone().isEmpty())
            {
                QTreeWidgetItem* phoneItem = new QTreeWidgetItem(item);
                phoneItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x9e ") + member.phone());
                phoneItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
                phoneItem->setFlags(phoneItem->flags() & ~Qt::ItemIsSelectable);
                break;  // Only show first parent's phone
            }
        }

        // Address
        if (!family.address().isEmpty())
        {
            QTreeWidgetItem* addrItem = new QTreeWidgetItem(item);
            addrItem->setText(0, QString::fromUtf8("\xf0\x9f\x93\x8d ") + family.address().full());
            addrItem->setData(0, TypeRole, static_cast<int>(ItemType::ContactDetail));
            addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsSelectable);
        }
    }
}

void MinisteringView::addMinistersSection(QTreeWidgetItem* companionshipItem, const MinisteringGroup& group, bool isEQ)
{
    const QString& selectedId = isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType selectedType = isEQ ? m_eqSelectedType : m_rsSelectedType;

    const Document& doc = m_documentManager->document();

    // Create "Ministers" section header
    QTreeWidgetItem* ministersHeader = new QTreeWidgetItem(companionshipItem);
    ministersHeader->setText(0, tr("Ministers"));
    ministersHeader->setData(0, TypeRole, static_cast<int>(ItemType::SectionHeader));
    ministersHeader->setFlags(ministersHeader->flags() & ~Qt::ItemIsSelectable);

    // Style header italic, bold if selected
    QFont headerFont = ministersHeader->font(0);
    headerFont.setItalic(true);
    QString sectionId = group.id() + ":ministers";
    if (selectedId == sectionId && selectedType == ItemType::SectionHeader)
    {
        headerFont.setBold(true);
    }
    ministersHeader->setFont(0, headerFont);
    ministersHeader->setForeground(0, QColor(100, 100, 100));

    // Add individual ministers
    for (const QString& ministerId : group.ministerIds())
    {
        std::optional<Person> person = doc.findPersonById(ministerId);
        if (!person)
        {
            continue;
        }

        QTreeWidgetItem* ministerItem = new QTreeWidgetItem(ministersHeader);
        ministerItem->setText(0, person->displayName());
        ministerItem->setData(0, IdRole, ministerId);
        ministerItem->setData(0, TypeRole, static_cast<int>(ItemType::Minister));
        ministerItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

        // Bold if selected
        if (selectedId == ministerId && selectedType == ItemType::Minister)
        {
            QFont font = ministerItem->font(0);
            font.setBold(true);
            ministerItem->setFont(0, font);
        }
    }
}

void MinisteringView::addMinisteredSection(QTreeWidgetItem* companionshipItem, const MinisteringGroup& group, bool isEQ)
{
    const QString& selectedId = isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType selectedType = isEQ ? m_eqSelectedType : m_rsSelectedType;

    const Document& doc = m_documentManager->document();

    // Create section header - "Families" for EQ, "Sisters" for RS
    QTreeWidgetItem* ministeredHeader = new QTreeWidgetItem(companionshipItem);
    ministeredHeader->setText(0, isEQ ? tr("Families") : tr("Sisters"));
    ministeredHeader->setData(0, TypeRole, static_cast<int>(ItemType::SectionHeader));
    ministeredHeader->setFlags(ministeredHeader->flags() & ~Qt::ItemIsSelectable);

    // Style header italic, bold if selected
    QFont headerFont = ministeredHeader->font(0);
    headerFont.setItalic(true);
    QString sectionId = group.id() + ":ministered";
    if (selectedId == sectionId && selectedType == ItemType::SectionHeader)
    {
        headerFont.setBold(true);
    }
    ministeredHeader->setFont(0, headerFont);
    ministeredHeader->setForeground(0, QColor(100, 100, 100));

    if (isEQ)
    {
        // EQ: Add families
        const auto& families = doc.families();
        for (const QString& familyId : group.familyIds())
        {
            if (!families.contains(familyId))
            {
                continue;
            }

            const Family& family = families[familyId];
            QTreeWidgetItem* familyItem = new QTreeWidgetItem(ministeredHeader);
            familyItem->setText(0, family.displayName());
            familyItem->setData(0, IdRole, familyId);
            familyItem->setData(0, TypeRole, static_cast<int>(ItemType::MinisteredFamily));
            familyItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

            // Bold if selected
            if (selectedId == familyId && selectedType == ItemType::MinisteredFamily)
            {
                QFont font = familyItem->font(0);
                font.setBold(true);
                familyItem->setFont(0, font);
            }
        }
    }
    else
    {
        // RS: Add sisters
        for (const QString& personId : group.ministeredPersonIds())
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (!person)
            {
                continue;
            }

            QTreeWidgetItem* sisterItem = new QTreeWidgetItem(ministeredHeader);
            sisterItem->setText(0, person->displayName());
            sisterItem->setData(0, IdRole, personId);
            sisterItem->setData(0, TypeRole, static_cast<int>(ItemType::MinisteredSister));
            sisterItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

            // Bold if selected
            if (selectedId == personId && selectedType == ItemType::MinisteredSister)
            {
                QFont font = sisterItem->font(0);
                font.setBold(true);
                sisterItem->setFont(0, font);
            }
        }
    }
}

void MinisteringView::rebuildTreeImpl(QTreeWidget* tree, bool isEQ)
{
    tree->clear();

    const QString& selectedId = isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType selectedType = isEQ ? m_eqSelectedType : m_rsSelectedType;

    const Document& doc = m_documentManager->document();
    const auto& districts = isEQ ? doc.eqDistricts() : doc.rsDistricts();
    const auto& groups = isEQ ? doc.eqGroups() : doc.rsGroups();

    // Sort districts by name
    QList<MinisteringDistrict> sortedDistricts = districts.values();
    std::sort(sortedDistricts.begin(), sortedDistricts.end(),
              [](const MinisteringDistrict& a, const MinisteringDistrict& b)
              {
                  return a.name().toLower() < b.name().toLower();
              });

    for (const MinisteringDistrict& district : sortedDistricts)
    {
        // Count total families/persons in district
        int totalCount = 0;
        for (const QString& groupId : district.groupIds())
        {
            if (groups.contains(groupId))
            {
                const MinisteringGroup& group = groups[groupId];
                totalCount += isEQ ? group.familyCount() : group.ministeredPersonCount();
            }
        }

        QString districtText = QString("%1 (%2 %3)")
            .arg(district.name())
            .arg(totalCount)
            .arg(isEQ ? tr("families") : tr("sisters"));

        auto* districtItem = new QTreeWidgetItem();
        districtItem->setText(0, districtText);
        districtItem->setData(0, IdRole, district.id());
        districtItem->setData(0, TypeRole, static_cast<int>(ItemType::District));

        // Bold if selected
        if (selectedId == district.id() && selectedType == ItemType::District)
        {
            QFont font = districtItem->font(0);
            font.setBold(true);
            districtItem->setFont(0, font);
        }

        // Collect and sort companionships alphabetically by minister names
        QList<std::pair<QString, QString>> sortedGroups;  // (display text, group ID)
        for (const QString& groupId : district.groupIds())
        {
            if (!groups.contains(groupId))
            {
                continue;
            }

            const MinisteringGroup& group = groups[groupId];

            // Build minister names
            QStringList ministerNames;
            for (const QString& ministerId : group.ministerIds())
            {
                auto person = doc.findPersonById(ministerId);
                if (person)
                {
                    ministerNames.append(person->displayName());
                }
            }

            int count = isEQ ? group.familyCount() : group.ministeredPersonCount();
            QString companionshipText = QString("%1 (%2)")
                .arg(ministerNames.join(", "))
                .arg(count);

            sortedGroups.append({companionshipText, groupId});
        }

        std::sort(sortedGroups.begin(), sortedGroups.end(),
                  [](const auto& a, const auto& b)
                  {
                      return a.first.toLower() < b.first.toLower();
                  });

        // Add companionships under this district
        for (const auto& [companionshipText, groupId] : sortedGroups)
        {
            const MinisteringGroup& group = groups[groupId];

            auto* companionshipItem = new QTreeWidgetItem(districtItem);
            companionshipItem->setText(0, companionshipText);
            companionshipItem->setData(0, IdRole, group.id());
            companionshipItem->setData(0, TypeRole, static_cast<int>(ItemType::Companionship));

            // Bold if selected
            if (selectedId == group.id() && selectedType == ItemType::Companionship)
            {
                QFont font = companionshipItem->font(0);
                font.setBold(true);
                companionshipItem->setFont(0, font);
            }

            // Add ministers and ministered sections
            addMinistersSection(companionshipItem, group, isEQ);
            addMinisteredSection(companionshipItem, group, isEQ);
        }

        tree->addTopLevelItem(districtItem);
        districtItem->setExpanded(true);
    }
}

void MinisteringView::rebuildUnassignedTreeImpl(QTreeWidget* tree, bool isEQ)
{
    tree->clear();

    const QString& selectedId = isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType selectedType = isEQ ? m_eqSelectedType : m_rsSelectedType;

    const Document& doc = m_documentManager->document();

    if (isEQ)
    {
        QSet<QString> familyIds = unassignedFamilyIds();
        if (familyIds.isEmpty())
        {
            return;
        }

        // Create header item
        QTreeWidgetItem* headerItem = new QTreeWidgetItem();
        headerItem->setText(0, tr("Unassigned (%1 families)").arg(familyIds.size()));
        headerItem->setData(0, TypeRole, static_cast<int>(ItemType::UnassignedHeader));
        if (selectedId == "unassigned" && selectedType == ItemType::UnassignedHeader)
        {
            QFont font = headerItem->font(0);
            font.setBold(true);
            headerItem->setFont(0, font);
        }
        tree->addTopLevelItem(headerItem);

        // Collect and sort families by surname
        const auto& families = doc.families();
        QList<std::pair<QString, QString>> sortedFamilies;  // (display name, family ID)
        for (const QString& familyId : familyIds)
        {
            if (families.contains(familyId))
            {
                const Family& family = families[familyId];
                sortedFamilies.append({family.displayName(), familyId});
            }
        }
        std::sort(sortedFamilies.begin(), sortedFamilies.end(),
                  [](const auto& a, const auto& b) { return a.first.toLower() < b.first.toLower(); });

        // Add family items
        for (const auto& [displayName, familyId] : sortedFamilies)
        {
            QTreeWidgetItem* familyItem = new QTreeWidgetItem(headerItem);
            familyItem->setText(0, displayName);
            familyItem->setData(0, IdRole, familyId);
            familyItem->setData(0, TypeRole, static_cast<int>(ItemType::MinisteredFamily));
            familyItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

            // Bold if selected
            if (selectedId == familyId && selectedType == ItemType::MinisteredFamily)
            {
                QFont font = familyItem->font(0);
                font.setBold(true);
                familyItem->setFont(0, font);
            }
        }
    }
    else
    {
        QSet<QString> sisterIds = unassignedSisterIds();
        if (sisterIds.isEmpty())
        {
            return;
        }

        // Create header item
        QTreeWidgetItem* headerItem = new QTreeWidgetItem();
        headerItem->setText(0, tr("Unassigned (%1 sisters)").arg(sisterIds.size()));
        headerItem->setData(0, TypeRole, static_cast<int>(ItemType::UnassignedHeader));
        if (selectedId == "unassigned" && selectedType == ItemType::UnassignedHeader)
        {
            QFont font = headerItem->font(0);
            font.setBold(true);
            headerItem->setFont(0, font);
        }
        tree->addTopLevelItem(headerItem);

        // Collect and sort sisters by name
        QList<std::pair<QString, QString>> sortedSisters;  // (display name, person ID)
        for (const QString& personId : sisterIds)
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (person)
            {
                sortedSisters.append({person->displayName(), personId});
            }
        }
        std::sort(sortedSisters.begin(), sortedSisters.end(),
                  [](const auto& a, const auto& b) { return a.first.toLower() < b.first.toLower(); });

        // Add sister items
        for (const auto& [displayName, personId] : sortedSisters)
        {
            QTreeWidgetItem* sisterItem = new QTreeWidgetItem(headerItem);
            sisterItem->setText(0, displayName);
            sisterItem->setData(0, IdRole, personId);
            sisterItem->setData(0, TypeRole, static_cast<int>(ItemType::MinisteredSister));
            sisterItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

            // Bold if selected
            if (selectedId == personId && selectedType == ItemType::MinisteredSister)
            {
                QFont font = sisterItem->font(0);
                font.setBold(true);
                sisterItem->setFont(0, font);
            }
        }
    }
}

void MinisteringView::updateUnassignedVisibility()
{
    m_eqUnassignedTree->setVisible(m_isEQ && m_eqUnassignedTree->topLevelItemCount() > 0);
    m_rsUnassignedTree->setVisible(!m_isEQ && m_rsUnassignedTree->topLevelItemCount() > 0);
}

void MinisteringView::clearSelection(bool isEQ)
{
    if (isEQ)
    {
        m_eqSelectedId.clear();
    }
    else
    {
        m_rsSelectedId.clear();
    }
}

QSet<QString> MinisteringView::familyIdsForPersons(const QSet<QString>& personIds) const
{
    QSet<QString> familyIds;
    const Document& doc = m_documentManager->document();

    for (const QString& personId : personIds)
    {
        QString familyId = doc.familyIdForPerson(personId);
        if (!familyId.isEmpty())
        {
            familyIds.insert(familyId);
        }
    }

    return familyIds;
}

QSet<QString> MinisteringView::unassignedFamilyIds() const
{
    const Document& doc = m_documentManager->document();
    const auto& families = doc.families();
    const auto& groups = doc.eqGroups();

    QSet<QString> assignedIds;
    for (const auto& group : groups)
    {
        assignedIds.unite(group.familyIds());
    }

    QSet<QString> allIds;
    for (const auto& family : families)
    {
        allIds.insert(family.id());
    }

    return allIds - assignedIds;
}

QSet<QString> MinisteringView::unassignedSisterIds() const
{
    const Document& doc = m_documentManager->document();
    const auto& families = doc.families();
    const auto& groups = doc.rsGroups();

    QSet<QString> assignedPersonIds;
    for (const auto& group : groups)
    {
        assignedPersonIds.unite(group.ministeredPersonIds());
    }

    // Collect all adult female person IDs
    QSet<QString> allSisterIds;
    for (const auto& family : families)
    {
        for (const auto& member : family.members())
        {
            if (member.gender() == Gender::Female && member.isParent())
            {
                allSisterIds.insert(member.id());
            }
        }
    }

    return allSisterIds - assignedPersonIds;
}

// FamilyMarkerProvider interface implementation

HighlightInfo MinisteringView::highlightInfo() const
{
    HighlightInfo info;

    // Pick selection state based on current org
    const QString& selectedId = m_isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType selectedType = m_isEQ ? m_eqSelectedType : m_rsSelectedType;

    // No selection = no highlighting
    if (selectedId.isEmpty())
    {
        return info;
    }

    const Document& doc = m_documentManager->document();
    const auto& districts = m_isEQ ? doc.eqDistricts() : doc.rsDistricts();
    const auto& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();

    switch (selectedType)
    {
        case ItemType::UnassignedHeader:
        {
            if (m_isEQ)
            {
                info.highlightedFamilyIds = unassignedFamilyIds();
            }
            else
            {
                info.highlightedFamilyIds = familyIdsForPersons(unassignedSisterIds());
            }
            break;
        }

        case ItemType::District:
        {
            if (districts.contains(selectedId))
            {
                const MinisteringDistrict& district = districts[selectedId];
                for (const QString& groupId : district.groupIds())
                {
                    if (groups.contains(groupId))
                    {
                        const MinisteringGroup& group = groups[groupId];
                        if (m_isEQ)
                        {
                            info.highlightedFamilyIds.unite(group.familyIds());
                        }
                        else
                        {
                            info.highlightedFamilyIds.unite(familyIdsForPersons(group.ministeredPersonIds()));
                        }
                        info.contactPointFamilyIds.unite(familyIdsForPersons(group.ministerIds()));
                    }
                }
            }
            break;
        }

        case ItemType::Companionship:
        {
            if (groups.contains(selectedId))
            {
                const MinisteringGroup& group = groups[selectedId];
                if (m_isEQ)
                {
                    info.highlightedFamilyIds = group.familyIds();
                }
                else
                {
                    info.highlightedFamilyIds = familyIdsForPersons(group.ministeredPersonIds());
                }
                info.contactPointFamilyIds = familyIdsForPersons(group.ministerIds());
            }
            break;
        }

        case ItemType::SectionHeader:
        {
            // Parse composite ID: "{compId}:ministers" or "{compId}:ministered"
            int colonPos = selectedId.lastIndexOf(':');
            if (colonPos > 0)
            {
                QString compId = selectedId.left(colonPos);
                QString sectionType = selectedId.mid(colonPos + 1);

                if (groups.contains(compId))
                {
                    const MinisteringGroup& group = groups[compId];
                    if (sectionType == "ministers")
                    {
                        // Ministers section: highlight minister families with pips
                        QSet<QString> ministerFamilies = familyIdsForPersons(group.ministerIds());
                        info.highlightedFamilyIds = ministerFamilies;
                        info.contactPointFamilyIds = ministerFamilies;
                    }
                    else
                    {
                        // Ministered section: highlight ministered families
                        if (m_isEQ)
                        {
                            info.highlightedFamilyIds = group.familyIds();
                        }
                        else
                        {
                            info.highlightedFamilyIds = familyIdsForPersons(group.ministeredPersonIds());
                        }
                    }
                }
            }
            break;
        }

        case ItemType::Minister:
        {
            // Individual minister: highlight their family with pip
            QString familyId = doc.familyIdForPerson(selectedId);
            if (!familyId.isEmpty())
            {
                info.highlightedFamilyIds.insert(familyId);
                info.contactPointFamilyIds.insert(familyId);
            }
            break;
        }

        case ItemType::MinisteredFamily:
        {
            // Individual family: highlight that family
            info.highlightedFamilyIds.insert(selectedId);
            break;
        }

        case ItemType::MinisteredSister:
        {
            // Individual sister: highlight her family
            QString familyId = doc.familyIdForPerson(selectedId);
            if (!familyId.isEmpty())
            {
                info.highlightedFamilyIds.insert(familyId);
            }
            break;
        }

        case ItemType::ContactDetail:
            // Should never be selected, but handle gracefully
            break;
    }

    return info;
}

QSet<QString> MinisteringView::visibleFamilyIds() const
{
    // MinisteringView shows all families - visibility is controlled by opacity
    return {};  // Empty = show all
}
