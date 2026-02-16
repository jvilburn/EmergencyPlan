#include "UnassignedMinisteringModel.h"
#include "ContactIcons.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Filter.h"
#include "MinisteringGroup.h"
#include "Family.h"
#include "Person.h"

#include <algorithm>
#include <QSet>

UnassignedMinisteringModel::UnassignedMinisteringModel(DocumentManager* documentManager,
                                                         Filter* filter,
                                                         MinisteringOrg org,
                                                         QObject* parent)
    : BaseTreeModel(parent)
    , m_documentManager(documentManager)
    , m_filter(filter)
    , m_org(org)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &UnassignedMinisteringModel::onDocumentChanged);
    if (m_filter)
    {
        connect(m_filter, &Filter::changed, this, &UnassignedMinisteringModel::rebuild);
    }
    rebuild();
}

UnassignedMinisteringModel::~UnassignedMinisteringModel()
{
    clearNodes();
}

void UnassignedMinisteringModel::clearNodes()
{
    delete m_headerNode;
    m_headerNode = nullptr;
}

void UnassignedMinisteringModel::onDocumentChanged(const DocumentChange& change)
{
    // Full document reload
    if (change.action == ChangeAction::Full)
    {
        rebuild();
        return;
    }

    // Ministering group changes: full rebuild (affects who is unassigned)
    if (change.eqGroupId || change.rsGroupId)
    {
        rebuild();
        return;
    }

    // Family updated: refresh display text only
    if (change.familyId && change.action == ChangeAction::Updated)
    {
        refreshFamilyDisplayText(*change.familyId);
        return;
    }

    // Family added/removed: rebuild (affects unassigned list)
    if (change.familyId)
    {
        rebuild();
    }
}

void UnassignedMinisteringModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = m_documentManager->document();
    const QHash<MinisteringGroupId, MinisteringGroup>& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    if (isEQ())
    {
        // Find unassigned families
        QSet<FamilyId> assignedIds;
        for (const auto& group : groups)
        {
            assignedIds.unite(group.familyIds());
        }

        const QHash<FamilyId, Family>& families = doc.families();
        QSet<FamilyId> allIds;
        for (const auto& family : families)
        {
            allIds.insert(family.id());
        }

        QSet<FamilyId> unassignedIds = allIds - assignedIds;

        // Sort families by name, applying filter
        QList<QPair<QString, FamilyId>> sortedFamilies;
        for (const FamilyId& familyId : unassignedIds)
        {
            if (families.contains(familyId))
            {
                const Family& family = families[familyId];
                // Apply filter if set
                if (m_filter && !m_filter->passes(doc, family))
                {
                    continue;
                }
                sortedFamilies.append({family.displayName(), familyId});
            }
        }

        // Only create header if we have filtered results
        if (!sortedFamilies.isEmpty())
        {
            std::sort(sortedFamilies.begin(), sortedFamilies.end(),
                      [](const QPair<QString, FamilyId>& a, const QPair<QString, FamilyId>& b)
                      { return a.first.toLower() < b.first.toLower(); });

            // Create header with filtered count
            m_headerNode = new TreeNode();
            m_headerNode->type = ItemType::UnassignedHeader;
            m_headerNode->displayText = tr("Unassigned (%1 families)").arg(sortedFamilies.size());

            for (const QPair<QString, FamilyId>& familyData : sortedFamilies)
            {
                TreeNode* familyNode = new TreeNode();
                familyNode->type = ItemType::MinisteredFamily;
                familyNode->familyId = familyData.second;
                familyNode->displayText = familyData.first;
                familyNode->parent = m_headerNode;
                m_headerNode->children.append(familyNode);
            }
        }
    }
    else
    {
        // Find unassigned sisters
        QSet<PersonId> assignedPersonIds;
        for (const auto& group : groups)
        {
            assignedPersonIds.unite(group.ministeredPersonIds());
        }

        // Collect all adult female person IDs
        const QHash<FamilyId, Family>& families = doc.families();
        QSet<PersonId> allSisterIds;
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

        QSet<PersonId> unassignedIds = allSisterIds - assignedPersonIds;

        // Sort sisters by name, applying filter
        QList<QPair<QString, PersonId>> sortedSisters;
        for (const PersonId& personId : unassignedIds)
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (person)
            {
                // Apply filter if set
                if (m_filter && !m_filter->passes(doc, *person))
                {
                    continue;
                }
                sortedSisters.append({person->displayName(), personId});
            }
        }

        // Only create header if we have filtered results
        if (!sortedSisters.isEmpty())
        {
            std::sort(sortedSisters.begin(), sortedSisters.end(),
                      [](const QPair<QString, PersonId>& a, const QPair<QString, PersonId>& b)
                      { return a.first.toLower() < b.first.toLower(); });

            // Create header with filtered count
            m_headerNode = new TreeNode();
            m_headerNode->type = ItemType::UnassignedHeader;
            m_headerNode->displayText = tr("Unassigned (%1 sisters)").arg(sortedSisters.size());

            for (const QPair<QString, PersonId>& sisterData : sortedSisters)
            {
                TreeNode* sisterNode = new TreeNode();
                sisterNode->type = ItemType::MinisteredSister;
                sisterNode->personId = sisterData.second;
                sisterNode->displayText = sisterData.first;
                sisterNode->parent = m_headerNode;
                m_headerNode->children.append(sisterNode);
            }
        }
    }

    endResetModel();
}

UnassignedMinisteringModel::TreeNode* UnassignedMinisteringModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}

QModelIndex UnassignedMinisteringModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        // Top-level: header row
        if (row == 0 && m_headerNode)
        {
            return createIndex(row, 0, m_headerNode);
        }
        return QModelIndex();
    }

    TreeNode* parentNode = nodeFromIndex(parent);
    if (!parentNode)
    {
        return QModelIndex();
    }

    if (row >= 0 && row < parentNode->children.size())
    {
        return createIndex(row, 0, parentNode->children.at(row));
    }

    return QModelIndex();
}

QModelIndex UnassignedMinisteringModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // Parent is always the header (top-level)
    if (parentNode == m_headerNode)
    {
        return createIndex(0, 0, parentNode);
    }

    // Contact detail's parent is a family/sister
    int row = parentNode->parent->children.indexOf(parentNode);
    if (row >= 0)
    {
        return createIndex(row, 0, parentNode);
    }

    return QModelIndex();
}

int UnassignedMinisteringModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        // Root: 0 or 1 (header if we have unassigned items)
        return m_headerNode ? 1 : 0;
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return node->children.size();
    }

    return 0;
}

int UnassignedMinisteringModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool UnassignedMinisteringModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return m_headerNode != nullptr;
    }

    TreeNode* node = nodeFromIndex(parent);
    if (!node)
    {
        return false;
    }

    // Family/sister nodes can have contact children (lazy loaded)
    if (node->type == ItemType::MinisteredFamily
        || node->type == ItemType::MinisteredSister)
    {
        return true;
    }

    return !node->children.isEmpty();
}

QVariant UnassignedMinisteringModel::data(const QModelIndex& index, int role) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QVariant();
    }

    switch (role)
    {
    case Qt::DisplayRole:
        return node->displayText;

    case NodeTypeRole:
        return QVariant::fromValue(node->type);

    default:
        return QVariant();
    }
}

SelectionKey UnassignedMinisteringModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return SelectionKey::from(QString());
    }

    switch (node->type)
    {
    case ItemType::UnassignedHeader:
        return SelectionKey::literal("unassigned");
    case ItemType::MinisteredFamily:
        if (node->familyId)
        {
            return SelectionKey::from(*node->familyId);
        }
        return SelectionKey::from(QString());
    case ItemType::MinisteredSister:
        if (node->personId)
        {
            return SelectionKey::from(*node->personId);
        }
        return SelectionKey::from(QString());
    case ItemType::ContactDetail:
        return selectionKeyAt(index.parent());
    default:
        return SelectionKey::from(QString());
    }
}

QSet<FamilyId> UnassignedMinisteringModel::familyIdsForPersons(const QSet<PersonId>& personIds) const
{
    QSet<FamilyId> familyIds;
    const Document& doc = m_documentManager->document();
    for (const PersonId& personId : personIds)
    {
        std::optional<FamilyId> familyId = doc.familyIdForPerson(personId);
        if (familyId)
        {
            familyIds.insert(*familyId);
        }
    }
    return familyIds;
}

QSet<FamilyId> UnassignedMinisteringModel::unassignedFamilyIds() const
{
    QSet<FamilyId> familyIds;
    if (!m_headerNode)
    {
        return familyIds;
    }
    for (const TreeNode* child : m_headerNode->children)
    {
        if (child->type == ItemType::MinisteredFamily && child->familyId)
        {
            familyIds.insert(*child->familyId);
        }
    }
    return familyIds;
}

QSet<PersonId> UnassignedMinisteringModel::unassignedSisterIds() const
{
    QSet<PersonId> personIds;
    if (!m_headerNode)
    {
        return personIds;
    }
    for (const TreeNode* child : m_headerNode->children)
    {
        if (child->type == ItemType::MinisteredSister && child->personId)
        {
            personIds.insert(*child->personId);
        }
    }
    return personIds;
}

FamilyAssociation UnassignedMinisteringModel::relatedFamiliesAt(const QModelIndex& index) const
{
    FamilyAssociation assoc;
    if (!index.isValid())
    {
        return assoc;
    }

    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return assoc;
    }

    switch (node->type)
    {
    case ItemType::UnassignedHeader:
        if (isEQ())
        {
            assoc.relatedFamilyIds = unassignedFamilyIds();
        }
        else
        {
            assoc.relatedFamilyIds = familyIdsForPersons(unassignedSisterIds());
        }
        break;
    case ItemType::MinisteredFamily:
        if (node->familyId)
        {
            assoc.relatedFamilyIds.insert(*node->familyId);
        }
        break;
    case ItemType::MinisteredSister:
    {
        if (node->personId)
        {
            std::optional<FamilyId> fid = m_documentManager->document().familyIdForPerson(*node->personId);
            if (fid)
            {
                assoc.relatedFamilyIds.insert(*fid);
            }
        }
        break;
    }
    default:
        break;
    }

    return assoc;
}

ItemType UnassignedMinisteringModel::itemTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return ItemType::Invalid;
}

// Note: Similar logic exists in MinisteringModel::loadContactDetails().
// Kept separate per "no base class" design decision (see tree-view-model-migration.md).
void UnassignedMinisteringModel::loadContactDetails(const QModelIndex& index)
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->contactsLoaded)
    {
        return;
    }

    // Only load contacts for person/family nodes
    if (node->type != ItemType::MinisteredFamily && node->type != ItemType::MinisteredSister)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    int insertRow = node->children.size();

    if (node->type == ItemType::MinisteredSister)
    {
        // Person contact info
        if (!node->personId)
        {
            node->contactsLoaded = true;
            return;
        }

        std::optional<Person> person = doc.findPersonById(*node->personId);
        if (!person)
        {
            node->contactsLoaded = true;
            return;
        }

        // Check for address availability
        std::optional<FamilyId> familyId = doc.familyIdForPerson(*node->personId);
        const QHash<FamilyId, Family>& families = doc.families();
        bool hasAddress = false;
        if (familyId && families.contains(*familyId))
        {
            hasAddress = !families[*familyId].address().isEmpty();
        }

        // Count actual items to insert
        int itemCount = 0;
        if (!person->phone().isEmpty())
        {
            itemCount++;
        }
        if (!person->altPhone().isEmpty())
        {
            itemCount++;
        }
        if (!person->email().isEmpty())
        {
            itemCount++;
        }
        if (hasAddress)
        {
            itemCount++;
        }

        if (itemCount == 0)
        {
            node->contactsLoaded = true;
            return;
        }

        beginInsertRows(index, insertRow, insertRow + itemCount - 1);

        // Phone
        if (!person->phone().isEmpty())
        {
            TreeNode* phoneNode = new TreeNode();
            phoneNode->type = ItemType::ContactDetail;
            phoneNode->displayText = ContactIcons::Phone + person->phone();
            phoneNode->personId = node->personId;
            phoneNode->parent = node;
            node->children.append(phoneNode);
        }

        // Alt phone
        if (!person->altPhone().isEmpty())
        {
            TreeNode* altPhoneNode = new TreeNode();
            altPhoneNode->type = ItemType::ContactDetail;
            altPhoneNode->displayText = ContactIcons::Phone + person->altPhone() + tr(" (alt)");
            altPhoneNode->personId = node->personId;
            altPhoneNode->parent = node;
            node->children.append(altPhoneNode);
        }

        // Email
        if (!person->email().isEmpty())
        {
            TreeNode* emailNode = new TreeNode();
            emailNode->type = ItemType::ContactDetail;
            emailNode->displayText = ContactIcons::Email + person->email();
            emailNode->personId = node->personId;
            emailNode->parent = node;
            node->children.append(emailNode);
        }

        // Address (from family)
        if (hasAddress)
        {
            const Family& family = families[*familyId];
            TreeNode* addrNode = new TreeNode();
            addrNode->type = ItemType::ContactDetail;
            addrNode->displayText = ContactIcons::Address + family.address().full();
            addrNode->personId = node->personId;
            addrNode->parent = node;
            node->children.append(addrNode);
        }

        endInsertRows();
    }
    else if (node->type == ItemType::MinisteredFamily)
    {
        // Family contact info
        const QHash<FamilyId, Family>& families = doc.families();
        if (!node->familyId || !families.contains(*node->familyId))
        {
            node->contactsLoaded = true;
            return;
        }

        const Family& family = families[*node->familyId];

        // Count actual items to insert
        int itemCount = 0;
        bool hasParentPhone = false;
        for (const Person& member : family.members())
        {
            if (member.isParent() && !member.phone().isEmpty())
            {
                hasParentPhone = true;
                itemCount++;
                break;
            }
        }
        if (!family.address().isEmpty())
        {
            itemCount++;
        }

        if (itemCount == 0)
        {
            node->contactsLoaded = true;
            return;
        }

        beginInsertRows(index, insertRow, insertRow + itemCount - 1);

        // Find head of household for phone
        if (hasParentPhone)
        {
            for (const Person& member : family.members())
            {
                if (member.isParent() && !member.phone().isEmpty())
                {
                    TreeNode* phoneNode = new TreeNode();
                    phoneNode->type = ItemType::ContactDetail;
                    phoneNode->displayText = ContactIcons::Phone + member.phone();
                    phoneNode->familyId = node->familyId;
                    phoneNode->parent = node;
                    node->children.append(phoneNode);
                    break;
                }
            }
        }

        // Address
        if (!family.address().isEmpty())
        {
            TreeNode* addrNode = new TreeNode();
            addrNode->type = ItemType::ContactDetail;
            addrNode->displayText = ContactIcons::Address + family.address().full();
            addrNode->familyId = node->familyId;
            addrNode->parent = node;
            node->children.append(addrNode);
        }

        endInsertRows();
    }

    node->contactsLoaded = true;
}

bool UnassignedMinisteringModel::hasContactsLoaded(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->contactsLoaded;
    }
    return false;
}

bool UnassignedMinisteringModel::hasUnassigned() const
{
    return m_headerNode != nullptr;
}

void UnassignedMinisteringModel::refreshFamilyDisplayText(const FamilyId& familyId)
{
    if (!m_headerNode)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    const QHash<FamilyId, Family>& families = doc.families();

    if (!families.contains(familyId))
    {
        return;
    }

    const Family& family = families[familyId];

    // Build set of person IDs in this family for quick lookup
    QSet<PersonId> personIds;
    for (const Person& member : family.members())
    {
        personIds.insert(member.id());
    }

    // Walk children of header and update affected nodes
    for (int itemRow = 0; itemRow < m_headerNode->children.size(); ++itemRow)
    {
        TreeNode* itemNode = m_headerNode->children[itemRow];
        bool needsUpdate = false;

        if (itemNode->type == ItemType::MinisteredSister)
        {
            // Person node - check if person is in updated family
            if (itemNode->personId && personIds.contains(*itemNode->personId))
            {
                std::optional<Person> person = doc.findPersonById(*itemNode->personId);
                if (person)
                {
                    itemNode->displayText = person->displayName();
                    needsUpdate = true;
                }
            }
        }
        else if (itemNode->type == ItemType::MinisteredFamily)
        {
            // Family node - check if this is the updated family
            if (itemNode->familyId && *itemNode->familyId == familyId)
            {
                itemNode->displayText = family.displayName();
                needsUpdate = true;
            }
        }

        if (needsUpdate)
        {
            QModelIndex headerIndex = createIndex(0, 0, m_headerNode);
            QModelIndex itemIndex = index(itemRow, 0, headerIndex);
            emit dataChanged(itemIndex, itemIndex);
        }
    }
}
