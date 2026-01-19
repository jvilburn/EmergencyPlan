#include "MinisteringView.h"
#include "DocumentManager.h"
#include "Document.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "Family.h"
#include "Person.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QHeaderView>
#include <QEvent>

MinisteringView::MinisteringView(DocumentManager* docManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(docManager)
{
    setupUi();

    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &MinisteringView::onDocumentChanged);

    // Initial population
    regenerateColors();
    rebuildTree();
    updateUnassignedLabel();
}

void MinisteringView::setupUi()
{
    setMinimumWidth(250);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // EQ/RS toggle
    auto* toggleLayout = new QHBoxLayout();
    toggleLayout->setSpacing(0);

    auto* eqButton = new QPushButton(tr("EQ"));
    auto* rsButton = new QPushButton(tr("RS"));

    eqButton->setCheckable(true);
    rsButton->setCheckable(true);
    eqButton->setChecked(true);

    QString toggleStyle = R"(
        QPushButton {
            background: #e0e0e0;
            border: 1px solid #c0c0c0;
            padding: 6px 16px;
            font-weight: bold;
        }
        QPushButton:first-child { border-radius: 4px 0 0 4px; }
        QPushButton:last-child { border-radius: 0 4px 4px 0; border-left: none; }
        QPushButton:checked {
            background: #1976D2;
            color: white;
            border-color: #1565C0;
        }
    )";
    eqButton->setStyleSheet(toggleStyle + "QPushButton { border-radius: 4px 0 0 4px; }");
    rsButton->setStyleSheet(toggleStyle + "QPushButton { border-radius: 0 4px 4px 0; border-left: none; }");

    m_orgToggle = new QButtonGroup(this);
    m_orgToggle->addButton(eqButton, 0);
    m_orgToggle->addButton(rsButton, 1);
    m_orgToggle->setExclusive(true);

    toggleLayout->addWidget(eqButton);
    toggleLayout->addWidget(rsButton);
    toggleLayout->addStretch();

    layout->addLayout(toggleLayout);

    // Unassigned section
    m_unassignedLabel = new QLabel(tr("Unassigned (0)"));
    m_unassignedLabel->setStyleSheet(R"(
        QLabel {
            padding: 8px;
            background: #fff3e0;
            border: 1px solid #ffcc80;
            border-radius: 4px;
            color: #e65100;
            font-weight: bold;
        }
        QLabel:hover {
            background: #ffe0b2;
        }
    )");
    m_unassignedLabel->setCursor(Qt::PointingHandCursor);
    m_unassignedLabel->installEventFilter(this);
    layout->addWidget(m_unassignedLabel);

    // District/Companionship tree
    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);  // We handle selection ourselves
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
    )");
    layout->addWidget(m_tree, 1);

    // Connections
    connect(m_orgToggle, &QButtonGroup::idClicked,
            this, &MinisteringView::onOrgToggled);
    connect(m_tree, &QTreeWidget::itemClicked,
            this, &MinisteringView::onTreeItemClicked);
}

void MinisteringView::onOrgToggled(int id)
{
    m_isEQ = (id == 0);
    clearSelection();
    regenerateColors();
    rebuildTree();
    updateUnassignedLabel();
    emit highlightChanged();
}

void MinisteringView::onUnassignedClicked()
{
    m_unassignedSelected = !m_unassignedSelected;
    if (m_unassignedSelected)
    {
        m_selectedDistrictIds.clear();
        m_selectedCompanionshipIds.clear();
    }

    // Update visual state
    if (m_unassignedSelected)
    {
        m_unassignedLabel->setStyleSheet(R"(
            QLabel {
                padding: 8px;
                background: #e65100;
                border: 1px solid #bf360c;
                border-radius: 4px;
                color: white;
                font-weight: bold;
            }
        )");
    }
    else
    {
        m_unassignedLabel->setStyleSheet(R"(
            QLabel {
                padding: 8px;
                background: #fff3e0;
                border: 1px solid #ffcc80;
                border-radius: 4px;
                color: #e65100;
                font-weight: bold;
            }
            QLabel:hover {
                background: #ffe0b2;
            }
        )");
    }

    emit highlightChanged();
}

void MinisteringView::onTreeItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    QString itemId = item->data(0, IdRole).toString();
    ItemType type = static_cast<ItemType>(item->data(0, TypeRole).toInt());

    if (type == ItemType::District)
    {
        // Toggle district selection
        if (m_selectedDistrictIds.contains(itemId))
        {
            m_selectedDistrictIds.remove(itemId);
        }
        else
        {
            m_selectedDistrictIds.insert(itemId);
        }
        m_selectedCompanionshipIds.clear();
        m_unassignedSelected = false;
    }
    else if (type == ItemType::Companionship)
    {
        // Toggle companionship selection
        if (m_selectedCompanionshipIds.contains(itemId))
        {
            m_selectedCompanionshipIds.remove(itemId);
        }
        else
        {
            m_selectedCompanionshipIds.insert(itemId);
        }
        m_selectedDistrictIds.clear();
        m_unassignedSelected = false;
    }

    // Update visual selection in tree (bold selected items)
    rebuildTree();

    // Reset unassigned label style
    m_unassignedLabel->setStyleSheet(R"(
        QLabel {
            padding: 8px;
            background: #fff3e0;
            border: 1px solid #ffcc80;
            border-radius: 4px;
            color: #e65100;
            font-weight: bold;
        }
        QLabel:hover {
            background: #ffe0b2;
        }
    )");

    emit highlightChanged();
}

void MinisteringView::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change)
    // TODO: optimize for EqDistrict/EqGroup/RsDistrict/RsGroup scope changes
    regenerateColors();
    rebuildTree();
    updateUnassignedLabel();
    emit highlightChanged();
}

void MinisteringView::rebuildTree()
{
    m_tree->clear();

    const Document& doc = m_documentManager->document();
    const auto& districts = m_isEQ ? doc.eqDistricts() : doc.rsDistricts();
    const auto& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();

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
                totalCount += m_isEQ ? group.familyCount() : group.ministeredPersonCount();
            }
        }

        QString districtText = QString("%1 (%2 %3)")
            .arg(district.name())
            .arg(totalCount)
            .arg(m_isEQ ? tr("families") : tr("sisters"));

        auto* districtItem = new QTreeWidgetItem();
        districtItem->setText(0, districtText);
        districtItem->setData(0, IdRole, district.id());
        districtItem->setData(0, TypeRole, static_cast<int>(ItemType::District));

        // Bold if selected
        if (m_selectedDistrictIds.contains(district.id()))
        {
            QFont font = districtItem->font(0);
            font.setBold(true);
            districtItem->setFont(0, font);
        }

        // Add companionships under this district
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

            int count = m_isEQ ? group.familyCount() : group.ministeredPersonCount();
            QString companionshipText = QString("%1 (%2)")
                .arg(ministerNames.join(", "))
                .arg(count);

            auto* companionshipItem = new QTreeWidgetItem(districtItem);
            companionshipItem->setText(0, companionshipText);
            companionshipItem->setData(0, IdRole, group.id());
            companionshipItem->setData(0, TypeRole, static_cast<int>(ItemType::Companionship));

            // Add color swatch as decoration
            if (m_colorMap.contains(group.id()))
            {
                QPixmap swatch(12, 12);
                swatch.fill(m_colorMap[group.id()]);
                companionshipItem->setIcon(0, QIcon(swatch));
            }

            // Bold if selected
            if (m_selectedCompanionshipIds.contains(group.id()))
            {
                QFont font = companionshipItem->font(0);
                font.setBold(true);
                companionshipItem->setFont(0, font);
            }
        }

        m_tree->addTopLevelItem(districtItem);
        districtItem->setExpanded(true);
    }
}

void MinisteringView::regenerateColors()
{
    m_colorMap.clear();

    const Document& doc = m_documentManager->document();
    const auto& districts = m_isEQ ? doc.eqDistricts() : doc.rsDistricts();
    const auto& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();

    // Generate colors for companionships (more distinct)
    int groupCount = groups.size();
    int i = 0;
    for (const auto& group : groups)
    {
        float hue = static_cast<float>(360.0 * i) / static_cast<float>(qMax(1, groupCount));
        m_colorMap[group.id()] = QColor::fromHslF(hue / 360.0f, 0.7f, 0.5f);
        ++i;
    }

    // Generate colors for districts (fewer, more distinct)
    int districtCount = districts.size();
    i = 0;
    for (const auto& district : districts)
    {
        float hue = static_cast<float>(360.0 * i) / static_cast<float>(qMax(1, districtCount));
        m_colorMap[district.id()] = QColor::fromHslF(hue / 360.0f, 0.8f, 0.45f);
        ++i;
    }
}

void MinisteringView::updateUnassignedLabel()
{
    int count = 0;
    if (m_isEQ)
    {
        count = unassignedFamilyIds().size();
        m_unassignedLabel->setText(tr("Unassigned (%1 families)").arg(count));
    }
    else
    {
        count = unassignedSisterIds().size();
        m_unassignedLabel->setText(tr("Unassigned (%1 sisters)").arg(count));
    }

    m_unassignedLabel->setVisible(count > 0);
}

void MinisteringView::clearSelection()
{
    m_selectedDistrictIds.clear();
    m_selectedCompanionshipIds.clear();
    m_unassignedSelected = false;
}

QSet<QString> MinisteringView::selectedFamilyIds() const
{
    QSet<QString> result;
    const Document& doc = m_documentManager->document();
    const auto& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();
    const auto& districts = m_isEQ ? doc.eqDistricts() : doc.rsDistricts();

    if (m_unassignedSelected)
    {
        if (m_isEQ)
        {
            result = unassignedFamilyIds();
        }
        else
        {
            // RS: get family IDs for unassigned sisters
            result = familyIdsForPersons(unassignedSisterIds());
        }
    }
    else if (!m_selectedCompanionshipIds.isEmpty())
    {
        // Companionships selected
        for (const QString& groupId : m_selectedCompanionshipIds)
        {
            if (!groups.contains(groupId))
            {
                continue;
            }
            const MinisteringGroup& group = groups[groupId];
            if (m_isEQ)
            {
                result.unite(group.familyIds());
            }
            else
            {
                result.unite(familyIdsForPersons(group.ministeredPersonIds()));
            }
        }
    }
    else if (!m_selectedDistrictIds.isEmpty())
    {
        // Districts selected - get all families in all groups in those districts
        for (const QString& districtId : m_selectedDistrictIds)
        {
            if (!districts.contains(districtId))
            {
                continue;
            }
            const MinisteringDistrict& district = districts[districtId];
            for (const QString& groupId : district.groupIds())
            {
                if (!groups.contains(groupId))
                {
                    continue;
                }
                const MinisteringGroup& group = groups[groupId];
                if (m_isEQ)
                {
                    result.unite(group.familyIds());
                }
                else
                {
                    result.unite(familyIdsForPersons(group.ministeredPersonIds()));
                }
            }
        }
    }

    return result;
}

QSet<QString> MinisteringView::ministerFamilyIds() const
{
    QSet<QString> result;
    const Document& doc = m_documentManager->document();
    const auto& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();
    const auto& districts = m_isEQ ? doc.eqDistricts() : doc.rsDistricts();

    // Unassigned selection has no ministers
    if (m_unassignedSelected)
    {
        return result;
    }

    QSet<QString> ministerPersonIds;

    if (!m_selectedCompanionshipIds.isEmpty())
    {
        // Get ministers from selected companionships
        for (const QString& groupId : m_selectedCompanionshipIds)
        {
            if (groups.contains(groupId))
            {
                const MinisteringGroup& group = groups[groupId];
                ministerPersonIds.unite(group.ministerIds());
            }
        }
    }
    else if (!m_selectedDistrictIds.isEmpty())
    {
        // Get ministers from all groups in selected districts
        for (const QString& districtId : m_selectedDistrictIds)
        {
            if (!districts.contains(districtId))
            {
                continue;
            }
            const MinisteringDistrict& district = districts[districtId];
            for (const QString& groupId : district.groupIds())
            {
                if (groups.contains(groupId))
                {
                    const MinisteringGroup& group = groups[groupId];
                    ministerPersonIds.unite(group.ministerIds());
                }
            }
        }
    }

    // Convert minister person IDs to family IDs
    return familyIdsForPersons(ministerPersonIds);
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

// MapHighlightProvider interface implementation

QColor MinisteringView::familyColor(const QString& familyId) const
{
    const Document& doc = m_documentManager->document();
    const auto& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();
    const auto& districts = m_isEQ ? doc.eqDistricts() : doc.rsDistricts();

    // No selection - no colors
    if (m_selectedDistrictIds.isEmpty()
        && m_selectedCompanionshipIds.isEmpty()
        && !m_unassignedSelected)
    {
        return QColor();  // Invalid = default marker color
    }

    // Check if this family is selected (ministered) or is a minister
    QSet<QString> selected = selectedFamilyIds();
    QSet<QString> ministers = ministerFamilyIds();
    bool isHighlighted = selected.contains(familyId) || ministers.contains(familyId);
    if (!isHighlighted)
    {
        return QColor();  // Not highlighted = no color
    }

    // Find which companionship/district this family belongs to and return its color
    if (!m_selectedCompanionshipIds.isEmpty())
    {
        for (const QString& groupId : m_selectedCompanionshipIds)
        {
            if (!groups.contains(groupId))
            {
                continue;
            }
            const MinisteringGroup& group = groups[groupId];

            // Check if minister
            QSet<QString> ministerFamilies = familyIdsForPersons(group.ministerIds());
            if (ministerFamilies.contains(familyId) && m_colorMap.contains(groupId))
            {
                return m_colorMap[groupId];
            }

            // Check if ministered
            bool inGroup = false;
            if (m_isEQ)
            {
                inGroup = group.familyIds().contains(familyId);
            }
            else
            {
                inGroup = familyIdsForPersons(group.ministeredPersonIds()).contains(familyId);
            }
            if (inGroup && m_colorMap.contains(groupId))
            {
                return m_colorMap[groupId];
            }
        }
    }
    else if (!m_selectedDistrictIds.isEmpty())
    {
        // Check all groups in districts for minister or ministered
        for (const QString& districtId : m_selectedDistrictIds)
        {
            if (!districts.contains(districtId))
            {
                continue;
            }
            const MinisteringDistrict& district = districts[districtId];
            for (const QString& groupId : district.groupIds())
            {
                if (!groups.contains(groupId))
                {
                    continue;
                }
                const MinisteringGroup& group = groups[groupId];

                // Check if minister in this group
                QSet<QString> ministerFamilies = familyIdsForPersons(group.ministerIds());
                if (ministerFamilies.contains(familyId) && m_colorMap.contains(districtId))
                {
                    return m_colorMap[districtId];
                }

                // Check if ministered in this group
                bool inGroup = false;
                if (m_isEQ)
                {
                    inGroup = group.familyIds().contains(familyId);
                }
                else
                {
                    inGroup = familyIdsForPersons(group.ministeredPersonIds()).contains(familyId);
                }
                if (inGroup && m_colorMap.contains(districtId))
                {
                    return m_colorMap[districtId];
                }
            }
        }
    }
    else if (m_unassignedSelected)
    {
        return QColor("#FF9800");  // Orange for unassigned
    }

    return QColor();
}

qreal MinisteringView::familyOpacity(const QString& familyId) const
{
    // No selection - all visible at full opacity
    if (m_selectedDistrictIds.isEmpty()
        && m_selectedCompanionshipIds.isEmpty()
        && !m_unassignedSelected)
    {
        return 1.0;
    }

    // If something is selected, highlighted families (ministers + ministered) are full opacity
    QSet<QString> selected = selectedFamilyIds();
    QSet<QString> ministers = ministerFamilyIds();
    bool isHighlighted = selected.contains(familyId) || ministers.contains(familyId);
    return isHighlighted ? 1.0 : 0.3;
}

QSet<QString> MinisteringView::visibleFamilyIds() const
{
    // MinisteringView shows all families - visibility is controlled by opacity
    return {};  // Empty = show all
}

bool MinisteringView::isContactPoint(const QString& familyId) const
{
    // Ministers are contact points
    QSet<QString> ministers = ministerFamilyIds();
    return ministers.contains(familyId);
}

// Event filter for unassigned label click
bool MinisteringView::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_unassignedLabel && event->type() == QEvent::MouseButtonPress)
    {
        onUnassignedClicked();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}
