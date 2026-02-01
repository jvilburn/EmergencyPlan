#include "MinisteringView.h"
#include "DocumentManager.h"
#include "Document.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "UnassignedMinisteringModel.h"
#include "Family.h"
#include "Person.h"

#include <QFontMetrics>
#include <QVBoxLayout>
#include <QTabBar>
#include <QTreeView>
#include <QHeaderView>
#include <QItemSelectionModel>

MinisteringView::MinisteringView(DocumentManager* docManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(docManager)
{
    // Create models
    m_eqModel = new MinisteringModel(m_documentManager, true, this);
    m_rsModel = new MinisteringModel(m_documentManager, false, this);
    m_eqUnassignedModel = new UnassignedMinisteringModel(m_documentManager, true, this);
    m_rsUnassignedModel = new UnassignedMinisteringModel(m_documentManager, false, this);

    setupUi();

    // Connect model reset signals to update unassigned tree visibility
    connect(m_eqUnassignedModel, &QAbstractItemModel::modelReset,
            this, &MinisteringView::onUnassignedModelReset);
    connect(m_rsUnassignedModel, &QAbstractItemModel::modelReset,
            this, &MinisteringView::onUnassignedModelReset);

    updateUnassignedVisibility();
}

void MinisteringView::setupUi()
{
    setMinimumWidth(250);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // EQ/RS tabs
    m_orgTabs = new QTabBar();
    m_orgTabs->addTab(tr("Elders Quorum"));
    m_orgTabs->addTab(tr("Relief Society"));
    layout->addWidget(m_orgTabs);

    // Create all 4 trees
    m_eqUnassignedTree = createTreeView(true);
    m_rsUnassignedTree = createTreeView(true);
    m_eqTree = createTreeView(false);
    m_rsTree = createTreeView(false);

    // Set models
    m_eqTree->setModel(m_eqModel);
    m_rsTree->setModel(m_rsModel);
    m_eqUnassignedTree->setModel(m_eqUnassignedModel);
    m_rsUnassignedTree->setModel(m_rsUnassignedModel);

    // Expand districts by default
    m_eqTree->expandToDepth(0);
    m_rsTree->expandToDepth(0);

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

    // Main tree selection connections
    connect(m_eqTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MinisteringView::onSelectionChanged);
    connect(m_rsTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MinisteringView::onSelectionChanged);

    // Unassigned tree selection connections
    connect(m_eqUnassignedTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MinisteringView::onUnassignedSelectionChanged);
    connect(m_rsUnassignedTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MinisteringView::onUnassignedSelectionChanged);

    // Expand connections for lazy contact loading
    connect(m_eqTree, &QTreeView::expanded,
            this, &MinisteringView::onTreeExpanded);
    connect(m_rsTree, &QTreeView::expanded,
            this, &MinisteringView::onTreeExpanded);
    connect(m_eqUnassignedTree, &QTreeView::expanded,
            this, &MinisteringView::onUnassignedTreeExpanded);
    connect(m_rsUnassignedTree, &QTreeView::expanded,
            this, &MinisteringView::onUnassignedTreeExpanded);
    connect(m_eqUnassignedTree, &QTreeView::collapsed,
            this, &MinisteringView::onUnassignedTreeCollapsed);
    connect(m_rsUnassignedTree, &QTreeView::collapsed,
            this, &MinisteringView::onUnassignedTreeCollapsed);
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

void MinisteringView::onSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous)

    QItemSelectionModel* selModel = qobject_cast<QItemSelectionModel*>(sender());
    bool isEQ = (selModel == m_eqTree->selectionModel());
    MinisteringModel* model = isEQ ? m_eqModel : m_rsModel;

    QString& selectedId = isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType& selectedType = isEQ ? m_eqSelectedType : m_rsSelectedType;

    if (!current.isValid())
    {
        selectedId.clear();
        selectedType = ItemType::Invalid;
        emit highlightChanged();
        return;
    }

    ItemType type = model->itemTypeAt(current);

    // For ContactDetail nodes, use the parent's type and id for highlighting
    QModelIndex nodeToUse = current;
    if (type == ItemType::ContactDetail)
    {
        nodeToUse = current.parent();
        type = model->itemTypeAt(nodeToUse);
    }

    // Build the selection ID based on type
    QString itemId = model->idAt(nodeToUse);
    selectedId = itemId;
    selectedType = type;

    emit highlightChanged();
}

void MinisteringView::onUnassignedSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous)

    QItemSelectionModel* selModel = qobject_cast<QItemSelectionModel*>(sender());
    bool isEQ = (selModel == m_eqUnassignedTree->selectionModel());
    UnassignedMinisteringModel* model = isEQ ? m_eqUnassignedModel : m_rsUnassignedModel;

    QString& selectedId = isEQ ? m_eqSelectedId : m_rsSelectedId;
    ItemType& selectedType = isEQ ? m_eqSelectedType : m_rsSelectedType;

    if (!current.isValid())
    {
        selectedId.clear();
        selectedType = ItemType::Invalid;
        emit highlightChanged();
        return;
    }

    ItemType type = model->itemTypeAt(current);

    // Contact details are not interactive
    if (type == ItemType::ContactDetail)
    {
        return;
    }

    // Build the selection ID based on type
    QString itemId = model->idAt(current);
    selectedId = itemId;
    selectedType = type;

    emit highlightChanged();
}

void MinisteringView::onTreeExpanded(const QModelIndex& index)
{
    QTreeView* tree = qobject_cast<QTreeView*>(sender());
    bool isEQ = (tree == m_eqTree);
    MinisteringModel* model = isEQ ? m_eqModel : m_rsModel;

    ItemType type = model->itemTypeAt(index);

    if (type == ItemType::Companionship)
    {
        // Expand section headers (Ministers, Families/Sisters) when companionship is expanded
        int rowCount = model->rowCount(index);
        for (int i = 0; i < rowCount; ++i)
        {
            QModelIndex childIndex = model->index(i, 0, index);
            ItemType childType = model->itemTypeAt(childIndex);
            if (childType == ItemType::SectionHeader)
            {
                tree->expand(childIndex);
            }
        }
    }
    else if (type == ItemType::Minister
             || type == ItemType::MinisteredFamily
             || type == ItemType::MinisteredSister)
    {
        // Load contact info on expand
        model->loadContactDetails(index);
    }
}

void MinisteringView::onUnassignedTreeExpanded(const QModelIndex& index)
{
    QTreeView* tree = qobject_cast<QTreeView*>(sender());
    bool isEQ = (tree == m_eqUnassignedTree);
    UnassignedMinisteringModel* model = isEQ ? m_eqUnassignedModel : m_rsUnassignedModel;

    ItemType type = model->itemTypeAt(index);

    // Resize tree when header is expanded
    if (type == ItemType::UnassignedHeader)
    {
        tree->setFixedHeight(expandedTreeHeight());
    }
    // Load contact info on expand for families/sisters
    else if (type == ItemType::MinisteredFamily || type == ItemType::MinisteredSister)
    {
        model->loadContactDetails(index);
    }
}

void MinisteringView::onUnassignedTreeCollapsed(const QModelIndex& index)
{
    QTreeView* tree = qobject_cast<QTreeView*>(sender());
    bool isEQ = (tree == m_eqUnassignedTree);
    UnassignedMinisteringModel* model = isEQ ? m_eqUnassignedModel : m_rsUnassignedModel;

    ItemType type = model->itemTypeAt(index);

    // Shrink tree when header is collapsed
    if (type == ItemType::UnassignedHeader)
    {
        tree->setFixedHeight(collapsedTreeHeight());
    }
}

void MinisteringView::onUnassignedModelReset()
{
    updateUnassignedVisibility();

    // Reset height when model rebuilds (in case it was expanded before)
    int height = collapsedTreeHeight();
    m_eqUnassignedTree->setFixedHeight(height);
    m_rsUnassignedTree->setFixedHeight(height);
}

void MinisteringView::updateUnassignedVisibility()
{
    m_eqUnassignedTree->setVisible(m_isEQ && m_eqUnassignedModel->hasUnassigned());
    m_rsUnassignedTree->setVisible(!m_isEQ && m_rsUnassignedModel->hasUnassigned());
}

QTreeView* MinisteringView::createTreeView(bool isUnassigned)
{
    auto* tree = new QTreeView();
    tree->setHeaderHidden(true);
    tree->setRootIsDecorated(true);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setIndentation(16);
    if (isUnassigned)
    {
        tree->setFixedHeight(collapsedTreeHeight());
    }
    return tree;
}

int MinisteringView::collapsedTreeHeight() const
{
    // Calculate height for a single row plus margins
    QFontMetrics fm(font());
    int rowHeight = fm.height() + 8;  // Text height + padding
    return rowHeight + 16;  // Add margins
}

int MinisteringView::expandedTreeHeight() const
{
    // Calculate height for approximately 8 rows
    QFontMetrics fm(font());
    int rowHeight = fm.height() + 8;
    return rowHeight * 8 + 16;
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
    if (selectedId.isEmpty() || selectedType == ItemType::Invalid)
    {
        return info;
    }

    const Document& doc = m_documentManager->document();
    const auto& districts = m_isEQ ? doc.eqDistricts() : doc.rsDistricts();
    const auto& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();

    switch (selectedType)
    {
    case ItemType::Invalid:
        break;

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
