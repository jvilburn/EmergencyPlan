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
    // Structural changes: full rebuild
    if (change.scope == ChangeScope::Full
        || change.action == ChangeAction::BatchModified)
    {
        rebuild();
        return;
    }

    // Ministering group changes: full rebuild (affects who is unassigned)
    if (change.scope == ChangeScope::EqGroup
        || change.scope == ChangeScope::RsGroup)
    {
        rebuild();
        return;
    }

    // Family updated: refresh display text only
    if (change.scope == ChangeScope::Family
        && change.action == ChangeAction::Updated)
    {
        refreshFamilyDisplayText(change.entityId);
        return;
    }

    // Family added/removed: rebuild (affects unassigned list)
    if (change.scope == ChangeScope::Family)
    {
        rebuild();
    }
}

void UnassignedMinisteringModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = m_documentManager->document();
    const QHash<QString, MinisteringGroup>& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    if (isEQ())
    {
        // Find unassigned families
        QSet<QString> assignedIds;
        for (const auto& group : groups)
        {
            assignedIds.unite(group.familyIds());
        }

        const QHash<QString, Family>& families = doc.families();
        QSet<QString> allIds;
        for (const auto& family : families)
        {
            allIds.insert(family.id());
        }

        QSet<QString> unassignedIds = allIds - assignedIds;

        if (!unassignedIds.isEmpty())
        {
            // Create header
            m_headerNode = new TreeNode();
            m_headerNode->type = ItemType::UnassignedHeader;
            m_headerNode->id = "unassigned";
            m_headerNode->displayText = tr("Unassigned (%1 families)").arg(unassignedIds.size());

            // Sort families by name, applying filter
            QList<QPair<QString, QString>> sortedFamilies;
            for (const QString& familyId : unassignedIds)
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

            std::sort(sortedFamilies.begin(), sortedFamilies.end(),
                      [](const QPair<QString, QString>& a, const QPair<QString, QString>& b)
                      { return a.first.toLower() < b.first.toLower(); });

            for (const QPair<QString, QString>& familyData : sortedFamilies)
            {
                TreeNode* familyNode = new TreeNode();
                familyNode->type = ItemType::MinisteredFamily;
                familyNode->id = familyData.second;
                familyNode->displayText = familyData.first;
                familyNode->parent = m_headerNode;
                m_headerNode->children.append(familyNode);
            }
        }
    }
    else
    {
        // Find unassigned sisters
        QSet<QString> assignedPersonIds;
        for (const auto& group : groups)
        {
            assignedPersonIds.unite(group.ministeredPersonIds());
        }

        // Collect all adult female person IDs
        const QHash<QString, Family>& families = doc.families();
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

        QSet<QString> unassignedIds = allSisterIds - assignedPersonIds;

        if (!unassignedIds.isEmpty())
        {
            // Create header
            m_headerNode = new TreeNode();
            m_headerNode->type = ItemType::UnassignedHeader;
            m_headerNode->id = "unassigned";
            m_headerNode->displayText = tr("Unassigned (%1 sisters)").arg(unassignedIds.size());

            // Sort sisters by name, applying filter
            QList<QPair<QString, QString>> sortedSisters;
            for (const QString& personId : unassignedIds)
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

            std::sort(sortedSisters.begin(), sortedSisters.end(),
                      [](const QPair<QString, QString>& a, const QPair<QString, QString>& b)
                      { return a.first.toLower() < b.first.toLower(); });

            for (const QPair<QString, QString>& sisterData : sortedSisters)
            {
                TreeNode* sisterNode = new TreeNode();
                sisterNode->type = ItemType::MinisteredSister;
                sisterNode->id = sisterData.second;
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

    case IdRole:
        return node->id;

    case NodeTypeRole:
        return QVariant::fromValue(node->type);

    case SecondaryIdRole:
        return node->secondaryId;

    default:
        return QVariant();
    }
}

QString UnassignedMinisteringModel::idAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->id;
    }
    return QString();
}

QString UnassignedMinisteringModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    switch (node->type)
    {
    case ItemType::UnassignedHeader:
        return QStringLiteral("unassigned");
    case ItemType::MinisteredFamily:
        return node->id;  // Family ID - only appears once in unassigned list
    case ItemType::MinisteredSister:
        return node->id;  // Person ID - only appears once in unassigned list
    default:
        return QString();
    }
}

QSet<QString> UnassignedMinisteringModel::familyIdsForPersons(const QSet<QString>& personIds) const
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

QSet<QString> UnassignedMinisteringModel::unassignedFamilyIds() const
{
    // Collect all family IDs from tree nodes
    QSet<QString> familyIds;
    if (!m_headerNode)
    {
        return familyIds;
    }
    for (const TreeNode* child : m_headerNode->children)
    {
        if (child->type == ItemType::MinisteredFamily)
        {
            familyIds.insert(child->id);
        }
    }
    return familyIds;
}

QSet<QString> UnassignedMinisteringModel::unassignedSisterIds() const
{
    // Collect all person IDs from tree nodes
    QSet<QString> personIds;
    if (!m_headerNode)
    {
        return personIds;
    }
    for (const TreeNode* child : m_headerNode->children)
    {
        if (child->type == ItemType::MinisteredSister)
        {
            personIds.insert(child->id);
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

    ItemType type = itemTypeAt(index);
    QString id = idAt(index);

    switch (type)
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
        assoc.relatedFamilyIds.insert(id);
        break;
    case ItemType::MinisteredSister:
    {
        QString familyId = m_documentManager->document().familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            assoc.relatedFamilyIds.insert(familyId);
        }
        break;
    }
    default:
        break;
    }

    // No contact points in unassigned list
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
        std::optional<Person> person = doc.findPersonById(node->id);
        if (!person)
        {
            node->contactsLoaded = true;
            return;
        }

        // Check for address availability
        QString familyId = doc.familyIdForPerson(node->id);
        const QHash<QString, Family>& families = doc.families();
        bool hasAddress = false;
        if (!familyId.isEmpty() && families.contains(familyId))
        {
            hasAddress = !families[familyId].address().isEmpty();
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
            phoneNode->secondaryId = node->id;
            phoneNode->parent = node;
            node->children.append(phoneNode);
        }

        // Alt phone
        if (!person->altPhone().isEmpty())
        {
            TreeNode* altPhoneNode = new TreeNode();
            altPhoneNode->type = ItemType::ContactDetail;
            altPhoneNode->displayText = ContactIcons::Phone + person->altPhone() + tr(" (alt)");
            altPhoneNode->secondaryId = node->id;
            altPhoneNode->parent = node;
            node->children.append(altPhoneNode);
        }

        // Email
        if (!person->email().isEmpty())
        {
            TreeNode* emailNode = new TreeNode();
            emailNode->type = ItemType::ContactDetail;
            emailNode->displayText = ContactIcons::Email + person->email();
            emailNode->secondaryId = node->id;
            emailNode->parent = node;
            node->children.append(emailNode);
        }

        // Address (from family)
        if (hasAddress)
        {
            const Family& family = families[familyId];
            TreeNode* addrNode = new TreeNode();
            addrNode->type = ItemType::ContactDetail;
            addrNode->displayText = ContactIcons::Address + family.address().full();
            addrNode->secondaryId = node->id;
            addrNode->parent = node;
            node->children.append(addrNode);
        }

        endInsertRows();
    }
    else if (node->type == ItemType::MinisteredFamily)
    {
        // Family contact info
        const QHash<QString, Family>& families = doc.families();
        if (!families.contains(node->id))
        {
            node->contactsLoaded = true;
            return;
        }

        const Family& family = families[node->id];

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
                    phoneNode->secondaryId = node->id;
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
            addrNode->secondaryId = node->id;
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

void UnassignedMinisteringModel::refreshFamilyDisplayText(const QString& familyId)
{
    if (!m_headerNode)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    const QHash<QString, Family>& families = doc.families();

    if (!families.contains(familyId))
    {
        return;
    }

    const Family& family = families[familyId];

    // Build set of person IDs in this family for quick lookup
    QSet<QString> personIds;
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
            if (personIds.contains(itemNode->id))
            {
                std::optional<Person> person = doc.findPersonById(itemNode->id);
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
            if (itemNode->id == familyId)
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
