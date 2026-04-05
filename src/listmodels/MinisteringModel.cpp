#include "MinisteringModel.h"
#include "ContactIcons.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyManager.h"
#include "Filter.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "Family.h"
#include "Person.h"
#include "StatusIcons.h"

#include <algorithm>
#include <QFont>

MinisteringModel::MinisteringModel(Filter* filter,
                                     MinisteringOrg org,
                                     QObject* parent)
    : BaseTreeModel(parent)
    , m_filter(filter)
    , m_org(org)
{
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &MinisteringModel::onDocumentChanged);
    if (m_filter)
    {
        connect(m_filter, &Filter::changed, this, &MinisteringModel::rebuild);
    }
    connect(EmergencyManager::instance(), &EmergencyManager::familyStatusChanged,
            this, &MinisteringModel::onFamilyStatusChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyStarted,
            this, &MinisteringModel::onEmergencyStateChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::emergencyEnded,
            this, &MinisteringModel::onEmergencyStateChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::archiveViewOpened,
            this, &MinisteringModel::onEmergencyStateChanged);
    connect(EmergencyManager::instance(), &EmergencyManager::archiveViewClosed,
            this, &MinisteringModel::onEmergencyStateChanged);
    rebuild();
}

MinisteringModel::~MinisteringModel()
{
    clearNodes();
}

void MinisteringModel::clearNodes()
{
    qDeleteAll(m_districtNodes);
    m_districtNodes.clear();
}

void MinisteringModel::onDocumentChanged(const DocumentChange& change)
{
    // Full document reload
    if (change.action == ChangeAction::Full)
    {
        rebuild();
        return;
    }

    // Ministering structure changes: full rebuild
    if (change.eqDistrictId || change.eqGroupId
        || change.rsDistrictId || change.rsGroupId)
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

    // Family added/removed: rebuild
    if (change.familyId)
    {
        rebuild();
    }
}

void MinisteringModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = DocumentManager::instance()->document();
    const QHash<MinisteringDistrictId, MinisteringDistrict>& districts = isEQ() ? doc.eqDistricts() : doc.rsDistricts();
    const QHash<MinisteringGroupId, MinisteringGroup>& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    // Sort districts by name
    QList<MinisteringDistrict> sortedDistricts = districts.values();
    std::sort(sortedDistricts.begin(), sortedDistricts.end(),
              [](const MinisteringDistrict& a, const MinisteringDistrict& b)
              { return a.name().toLower() < b.name().toLower(); });

    for (const MinisteringDistrict& district : sortedDistricts)
    {
        TreeNode* districtNode = new TreeNode();
        districtNode->type = ItemType::District;
        districtNode->districtId = district.id();
        // Display text will be updated after filtering

        // Collect companionships with minister names for sorting
        QList<QPair<QString, MinisteringGroupId>> sortedGroups;  // (minister names, group ID)
        for (const MinisteringGroupId& groupId : district.groupIds())
        {
            if (!groups.contains(groupId))
            {
                continue;
            }

            const MinisteringGroup& group = groups[groupId];

            // Build minister names
            QStringList ministerNames;
            for (const PersonId& ministerId : group.ministerIds())
            {
                std::optional<Person> person = doc.findPersonById(ministerId);
                if (person)
                {
                    ministerNames.append(person->displayName());
                }
            }

            sortedGroups.append({ministerNames.join(", "), groupId});
        }

        std::sort(sortedGroups.begin(), sortedGroups.end(),
                  [](const QPair<QString, MinisteringGroupId>& a, const QPair<QString, MinisteringGroupId>& b)
                  { return a.first.toLower() < b.first.toLower(); });

        // Add companionships under this district
        int districtCount = 0;
        for (const QPair<QString, MinisteringGroupId>& groupData : sortedGroups)
        {
            const QString& ministerNames = groupData.first;
            const MinisteringGroupId& groupId = groupData.second;

            TreeNode* companionshipNode = new TreeNode();
            companionshipNode->type = ItemType::Companionship;
            companionshipNode->groupId = groupId;
            // Display text will be updated after filtering
            companionshipNode->parent = districtNode;
            districtNode->children.append(companionshipNode);

            // Add ministers and ministered sections
            addMinistersSection(companionshipNode, groupId);
            addMinisteredSection(companionshipNode, groupId);

            // Count filtered items and update companionship text
            int filteredCount = countMinisteredChildren(companionshipNode);
            companionshipNode->displayText = formatCompanionshipText(
                ministerNames, filteredCount, companionshipNode);

            districtCount += filteredCount;
        }

        districtNode->displayText = formatDistrictText(district, districtCount, districtNode);

        m_districtNodes.append(districtNode);
    }

    endResetModel();
}

void MinisteringModel::addMinistersSection(TreeNode* companionshipNode, const MinisteringGroupId& groupId)
{
    const Document& doc = DocumentManager::instance()->document();
    const QHash<MinisteringGroupId, MinisteringGroup>& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    if (!groups.contains(groupId))
    {
        return;
    }

    const MinisteringGroup& group = groups[groupId];

    // Create "Ministers" section header
    TreeNode* ministersHeader = new TreeNode();
    ministersHeader->type = ItemType::SectionHeader;
    ministersHeader->groupId = groupId;
    ministersHeader->displayText = tr("Ministers");
    ministersHeader->parent = companionshipNode;
    companionshipNode->children.append(ministersHeader);

    // Collect and sort ministers by name
    QList<QPair<QString, PersonId>> sortedMinisters;  // (display name, person ID)
    for (const PersonId& ministerId : group.ministerIds())
    {
        std::optional<Person> person = doc.findPersonById(ministerId);
        if (person)
        {
            sortedMinisters.append({person->displayName(), ministerId});
        }
    }

    std::sort(sortedMinisters.begin(), sortedMinisters.end(),
              [](const QPair<QString, PersonId>& a, const QPair<QString, PersonId>& b)
              { return a.first.toLower() < b.first.toLower(); });

    // Add individual ministers (with phone if emergency active)
    for (const QPair<QString, PersonId>& ministerData : sortedMinisters)
    {
        TreeNode* ministerNode = new TreeNode();
        ministerNode->type = ItemType::Minister;
        ministerNode->personId = ministerData.second;

        // Append phone number during emergency
        if (EmergencyManager::instance() && EmergencyManager::instance()->isActive())
        {
            std::optional<Person> p = doc.findPersonById(ministerData.second);
            if (p && !p->phone().isEmpty())
            {
                ministerNode->displayText = QString("%1  %2%3")
                    .arg(ministerData.first)
                    .arg(ContactIcons::Phone)
                    .arg(p->phone());
            }
            else
            {
                ministerNode->displayText = ministerData.first;
            }
        }
        else
        {
            ministerNode->displayText = ministerData.first;
        }

        ministerNode->parent = ministersHeader;
        ministersHeader->children.append(ministerNode);
    }
}

void MinisteringModel::addMinisteredSection(TreeNode* companionshipNode, const MinisteringGroupId& groupId)
{
    const Document& doc = DocumentManager::instance()->document();
    const QHash<MinisteringGroupId, MinisteringGroup>& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    if (!groups.contains(groupId))
    {
        return;
    }

    const MinisteringGroup& group = groups[groupId];

    // Create section header - "Families" for EQ, "Sisters" for RS
    TreeNode* ministeredHeader = new TreeNode();
    ministeredHeader->type = ItemType::SectionHeader;
    ministeredHeader->groupId = groupId;
    ministeredHeader->displayText = isEQ() ? tr("Families") : tr("Sisters");
    ministeredHeader->parent = companionshipNode;
    companionshipNode->children.append(ministeredHeader);

    if (isEQ())
    {
        // EQ: Add families
        const QHash<FamilyId, Family>& families = doc.families();

        // Collect and sort families by name
        QList<QPair<QString, FamilyId>> sortedFamilies;  // (display name, family ID)
        for (const FamilyId& familyId : group.familyIds())
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
                  [](const QPair<QString, FamilyId>& a, const QPair<QString, FamilyId>& b)
                  { return a.first.toLower() < b.first.toLower(); });

        for (const QPair<QString, FamilyId>& familyData : sortedFamilies)
        {
            TreeNode* familyNode = new TreeNode();
            familyNode->type = ItemType::MinisteredFamily;
            familyNode->familyId = familyData.second;
            familyNode->displayText = familyData.first;
            familyNode->parent = ministeredHeader;
            ministeredHeader->children.append(familyNode);
        }
    }
    else
    {
        // RS: Add sisters
        QList<QPair<QString, PersonId>> sortedSisters;  // (display name, person ID)
        for (const PersonId& personId : group.ministeredPersonIds())
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
                  [](const QPair<QString, PersonId>& a, const QPair<QString, PersonId>& b)
                  { return a.first.toLower() < b.first.toLower(); });

        for (const QPair<QString, PersonId>& sisterData : sortedSisters)
        {
            TreeNode* sisterNode = new TreeNode();
            sisterNode->type = ItemType::MinisteredSister;
            sisterNode->personId = sisterData.second;
            sisterNode->displayText = sisterData.first;
            sisterNode->parent = ministeredHeader;
            ministeredHeader->children.append(sisterNode);
        }
    }
}

MinisteringModel::TreeNode* MinisteringModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}

QModelIndex MinisteringModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        // Top-level: district rows
        if (row >= 0 && row < m_districtNodes.size())
        {
            return createIndex(row, 0, m_districtNodes.at(row));
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

QModelIndex MinisteringModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // Find parent's row in its own parent's children list
    if (!parentNode->parent)
    {
        // Parent is a district (top-level)
        int row = m_districtNodes.indexOf(parentNode);
        if (row >= 0)
        {
            return createIndex(row, 0, parentNode);
        }
    }
    else
    {
        // Parent is nested
        int row = parentNode->parent->children.indexOf(parentNode);
        if (row >= 0)
        {
            return createIndex(row, 0, parentNode);
        }
    }

    return QModelIndex();
}

int MinisteringModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        // Root: number of districts
        return m_districtNodes.size();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return node->children.size();
    }

    return 0;
}

int MinisteringModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool MinisteringModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_districtNodes.isEmpty();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (!node)
    {
        return false;
    }

    // Person/family nodes can have contact children (lazy loaded)
    if (node->type == ItemType::Minister
        || node->type == ItemType::MinisteredFamily
        || node->type == ItemType::MinisteredSister)
    {
        return true;
    }

    return !node->children.isEmpty();
}

QVariant MinisteringModel::data(const QModelIndex& index, int role) const
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

    case Qt::DecorationRole:
        if (EmergencyManager::instance() && EmergencyManager::instance()->isActive())
        {
            if (node->type == ItemType::MinisteredFamily
                || node->type == ItemType::MinisteredSister)
            {
                FamilyId fid = familyIdForNode(node);
                if (!fid.isNull())
                {
                    EffectiveContactStatus status = EmergencyManager::instance()->familyStatus(fid);
                    if (status != EffectiveContactStatus::NotContacted)
                    {
                        return StatusIcons::iconForStatus(status);
                    }
                }
            }
        }
        return QVariant();

    case Qt::FontRole:
        {
            // Section headers are italic
            if (node->type == ItemType::SectionHeader)
            {
                QFont font;
                font.setItalic(true);
                return font;
            }
        }
        return QVariant();

    case Qt::ForegroundRole:
        {
            // Section headers are gray
            if (node->type == ItemType::SectionHeader)
            {
                return QColor(100, 100, 100);
            }
        }
        return QVariant();

    case NodeTypeRole:
        return QVariant::fromValue(node->type);

    default:
        return QVariant();
    }
}

SelectionKey MinisteringModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return SelectionKey::literal(QString());
    }

    switch (node->type)
    {
    case ItemType::District:
        if (node->districtId)
        {
            return SelectionKey::from(*node->districtId);
        }
        return SelectionKey::literal(QString());
    case ItemType::Companionship:
        if (node->groupId)
        {
            return SelectionKey::from(*node->groupId);
        }
        return SelectionKey::literal(QString());
    case ItemType::SectionHeader:
    {
        // Determine if this is the ministers or ministered section
        // Ministers section is always first child of companionship
        if (node->groupId && node->parent && node->parent->type == ItemType::Companionship)
        {
            QString suffix = isMinistersSection(node) ? "ministers" : "ministered";
            return SelectionKey::literal(node->groupId->toString() + ":" + suffix);
        }
        return SelectionKey::literal(QString());
    }
    case ItemType::Minister:
    {
        auto compId = companionshipIdAt(index);
        if (compId && node->personId)
        {
            return SelectionKey::literal(compId->toString() + ":minister:" + node->personId->toString());
        }
        return SelectionKey::literal(QString());
    }
    case ItemType::MinisteredFamily:
    {
        auto compId = companionshipIdAt(index);
        if (compId && node->familyId)
        {
            return SelectionKey::literal(compId->toString() + ":family:" + node->familyId->toString());
        }
        return SelectionKey::literal(QString());
    }
    case ItemType::MinisteredSister:
    {
        auto compId = companionshipIdAt(index);
        if (compId && node->personId)
        {
            return SelectionKey::literal(compId->toString() + ":sister:" + node->personId->toString());
        }
        return SelectionKey::literal(QString());
    }
    case ItemType::ContactDetail:
        return selectionKeyAt(index.parent());
    default:
        return SelectionKey::literal(QString());
    }
}

QSet<FamilyId> MinisteringModel::familyIdsForPersons(const QSet<PersonId>& personIds) const
{
    QSet<FamilyId> familyIds;
    const Document& doc = DocumentManager::instance()->document();
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

int MinisteringModel::countMinisteredChildren(TreeNode* companionshipNode) const
{
    TreeNode* section = findMinisteredSection(companionshipNode);
    if (section)
    {
        return section->children.size();
    }
    return 0;
}

bool MinisteringModel::isMinistersSection(const TreeNode* sectionNode) const
{
    return sectionNode->parent
           && sectionNode->parent->type == ItemType::Companionship
           && !sectionNode->parent->children.isEmpty()
           && sectionNode->parent->children.first() == sectionNode;
}

FamilyAssociation MinisteringModel::relatedFamiliesAt(const QModelIndex& index) const
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

    const Document& doc = DocumentManager::instance()->document();
    const auto& districts = isEQ() ? doc.eqDistricts() : doc.rsDistricts();
    const auto& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    switch (node->type)
    {
    case ItemType::District:
        if (node->districtId && districts.contains(*node->districtId))
        {
            const MinisteringDistrict& district = districts[*node->districtId];
            for (const MinisteringGroupId& groupId : district.groupIds())
            {
                if (groups.contains(groupId))
                {
                    const MinisteringGroup& group = groups[groupId];
                    if (isEQ())
                    {
                        assoc.relatedFamilyIds.unite(group.familyIds());
                    }
                    else
                    {
                        assoc.relatedFamilyIds.unite(familyIdsForPersons(group.ministeredPersonIds()));
                    }
                    assoc.contactPointFamilyIds.unite(familyIdsForPersons(group.ministerIds()));
                }
            }
        }
        break;

    case ItemType::Companionship:
        if (node->groupId && groups.contains(*node->groupId))
        {
            const MinisteringGroup& group = groups[*node->groupId];
            if (isEQ())
            {
                assoc.relatedFamilyIds = group.familyIds();
            }
            else
            {
                assoc.relatedFamilyIds = familyIdsForPersons(group.ministeredPersonIds());
            }
            assoc.contactPointFamilyIds = familyIdsForPersons(group.ministerIds());
        }
        break;

    case ItemType::SectionHeader:
    {
        if (node->groupId && groups.contains(*node->groupId))
        {
            const MinisteringGroup& group = groups[*node->groupId];
            if (isMinistersSection(node))
            {
                QSet<FamilyId> ministerFamilies = familyIdsForPersons(group.ministerIds());
                assoc.relatedFamilyIds = ministerFamilies;
                assoc.contactPointFamilyIds = ministerFamilies;
            }
            else
            {
                if (isEQ())
                {
                    assoc.relatedFamilyIds = group.familyIds();
                }
                else
                {
                    assoc.relatedFamilyIds = familyIdsForPersons(group.ministeredPersonIds());
                }
            }
        }
        break;
    }

    case ItemType::Minister:
    {
        if (node->personId)
        {
            std::optional<FamilyId> fid = doc.familyIdForPerson(*node->personId);
            if (fid)
            {
                assoc.relatedFamilyIds.insert(*fid);
                assoc.contactPointFamilyIds.insert(*fid);
            }
        }
        break;
    }

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
            std::optional<FamilyId> fid = doc.familyIdForPerson(*node->personId);
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

ItemType MinisteringModel::itemTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return ItemType::Invalid;
}

QModelIndex MinisteringModel::indexForFamilyId(const FamilyId& familyId) const
{
    const Document& doc = DocumentManager::instance()->document();

    for (int d = 0; d < m_districtNodes.size(); ++d)
    {
        TreeNode* districtNode = m_districtNodes[d];
        for (int c = 0; c < districtNode->children.size(); ++c)
        {
            TreeNode* compNode = districtNode->children[c];
            for (int s = 0; s < compNode->children.size(); ++s)
            {
                TreeNode* sectionNode = compNode->children[s];
                if (isMinistersSection(sectionNode))
                {
                    continue;
                }
                for (int i = 0; i < sectionNode->children.size(); ++i)
                {
                    TreeNode* itemNode = sectionNode->children[i];
                    if (itemNode->type == ItemType::MinisteredFamily
                        && itemNode->familyId && *itemNode->familyId == familyId)
                    {
                        return createIndex(i, 0, itemNode);
                    }
                    if (itemNode->type == ItemType::MinisteredSister
                        && itemNode->personId)
                    {
                        std::optional<FamilyId> fid = doc.familyIdForPerson(*itemNode->personId);
                        if (fid && *fid == familyId)
                        {
                            return createIndex(i, 0, itemNode);
                        }
                    }
                }
            }
        }
    }
    return {};
}

QModelIndex MinisteringModel::indexForMinisterByFamilyId(const FamilyId& familyId) const
{
    const Document& doc = DocumentManager::instance()->document();

    for (int d = 0; d < m_districtNodes.size(); ++d)
    {
        TreeNode* districtNode = m_districtNodes[d];
        for (int c = 0; c < districtNode->children.size(); ++c)
        {
            TreeNode* compNode = districtNode->children[c];
            for (int s = 0; s < compNode->children.size(); ++s)
            {
                TreeNode* sectionNode = compNode->children[s];
                if (!isMinistersSection(sectionNode))
                {
                    continue;
                }
                for (int i = 0; i < sectionNode->children.size(); ++i)
                {
                    TreeNode* itemNode = sectionNode->children[i];
                    if (itemNode->type == ItemType::Minister && itemNode->personId)
                    {
                        std::optional<FamilyId> fid = doc.familyIdForPerson(*itemNode->personId);
                        if (fid && *fid == familyId)
                        {
                            return createIndex(i, 0, itemNode);
                        }
                    }
                }
            }
        }
    }
    return {};
}

std::optional<MinisteringGroupId> MinisteringModel::companionshipIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);

    // Walk up to find the companionship
    while (node)
    {
        if (node->type == ItemType::Companionship && node->groupId)
        {
            return *node->groupId;
        }
        node = node->parent;
    }
    return std::nullopt;
}

// Note: Similar logic exists in UnassignedMinisteringModel::loadContactDetails().
// Kept separate per "no base class" design decision (see tree-view-model-migration.md).
void MinisteringModel::loadContactDetails(const QModelIndex& index)
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->contactsLoaded)
    {
        return;
    }

    // Only load contacts for person/family nodes
    if (node->type != ItemType::Minister
        && node->type != ItemType::MinisteredFamily
        && node->type != ItemType::MinisteredSister)
    {
        return;
    }

    const Document& doc = DocumentManager::instance()->document();
    int insertRow = node->children.size();

    if (node->type == ItemType::Minister || node->type == ItemType::MinisteredSister)
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

bool MinisteringModel::hasContactsLoaded(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->contactsLoaded;
    }
    return false;
}

void MinisteringModel::onFamilyStatusChanged(const FamilyId& familyId)
{
    // Find nodes for this family and emit dataChanged for DecorationRole
    // Also update parent companionship/district display text
    const Document& doc = DocumentManager::instance()->document();

    for (int d = 0; d < m_districtNodes.size(); ++d)
    {
        TreeNode* districtNode = m_districtNodes[d];
        bool districtAffected = false;

        for (int c = 0; c < districtNode->children.size(); ++c)
        {
            TreeNode* compNode = districtNode->children[c];
            if (compNode->type != ItemType::Companionship)
            {
                continue;
            }

            bool companionshipAffected = false;

            for (int s = 0; s < compNode->children.size(); ++s)
            {
                TreeNode* sectionNode = compNode->children[s];
                if (sectionNode->type != ItemType::SectionHeader
                    || isMinistersSection(sectionNode))
                {
                    continue;
                }

                for (int i = 0; i < sectionNode->children.size(); ++i)
                {
                    TreeNode* itemNode = sectionNode->children[i];
                    FamilyId fid = familyIdForNode(itemNode);
                    if (!fid.isNull() && fid == familyId)
                    {
                        QModelIndex sectionIndex = createIndex(s, 0, sectionNode);
                        QModelIndex itemIndex = index(i, 0, sectionIndex);
                        emit dataChanged(itemIndex, itemIndex, {Qt::DecorationRole});
                        companionshipAffected = true;
                    }
                }
            }

            if (companionshipAffected)
            {
                // Update companionship compact status
                int filteredCount = countMinisteredChildren(compNode);
                const QHash<MinisteringGroupId, MinisteringGroup>& groups =
                    isEQ() ? doc.eqGroups() : doc.rsGroups();

                if (compNode->groupId && groups.contains(*compNode->groupId))
                {
                    const MinisteringGroup& group = groups[*compNode->groupId];
                    QStringList ministerNames;
                    for (const PersonId& ministerId : group.ministerIds())
                    {
                        std::optional<Person> person = doc.findPersonById(ministerId);
                        if (person)
                        {
                            ministerNames.append(person->displayName());
                        }
                    }

                    compNode->displayText = formatCompanionshipText(
                        ministerNames.join(", "), filteredCount, compNode);

                    QModelIndex districtIndex = createIndex(d, 0, districtNode);
                    QModelIndex compIndex = index(c, 0, districtIndex);
                    emit dataChanged(compIndex, compIndex);
                }

                districtAffected = true;
            }
        }

        if (districtAffected)
        {
            // Recalculate district display text with updated progress
            const auto& districts = isEQ() ? doc.eqDistricts() : doc.rsDistricts();
            if (districtNode->districtId && districts.contains(*districtNode->districtId))
            {
                const MinisteringDistrict& dist = districts[*districtNode->districtId];
                int districtCount = 0;
                for (TreeNode* cn : districtNode->children)
                {
                    if (cn->type == ItemType::Companionship)
                    {
                        districtCount += countMinisteredChildren(cn);
                    }
                }
                districtNode->displayText = formatDistrictText(dist, districtCount, districtNode);
            }
            QModelIndex districtIndex = createIndex(d, 0, districtNode);
            emit dataChanged(districtIndex, districtIndex);
        }
    }
}

void MinisteringModel::onEmergencyStateChanged()
{
    rebuild();
}

MinisteringModel::TreeNode* MinisteringModel::findMinisteredSection(TreeNode* companionshipNode) const
{
    for (TreeNode* child : companionshipNode->children)
    {
        if (child->type == ItemType::SectionHeader && !isMinistersSection(child))
        {
            return child;
        }
    }
    return nullptr;
}

FamilyId MinisteringModel::familyIdForNode(const TreeNode* node) const
{
    if (node->type == ItemType::MinisteredFamily && node->familyId)
    {
        return *node->familyId;
    }
    if (node->type == ItemType::MinisteredSister && node->personId)
    {
        const Document& doc = DocumentManager::instance()->document();
        std::optional<FamilyId> fid = doc.familyIdForPerson(*node->personId);
        if (fid)
        {
            return *fid;
        }
    }
    return FamilyId();
}

QString MinisteringModel::compactStatusSummary(TreeNode* companionshipNode) const
{
    if (!EmergencyManager::instance() || !EmergencyManager::instance()->isActive())
    {
        return QString();
    }

    TreeNode* ministeredSection = findMinisteredSection(companionshipNode);
    if (!ministeredSection || ministeredSection->children.isEmpty())
    {
        return QString();
    }

    QString symbols;
    for (const TreeNode* child : ministeredSection->children)
    {
        FamilyId fid = familyIdForNode(child);
        if (fid.isNull())
        {
            symbols += StatusIcons::Circle;
            continue;
        }

        EffectiveContactStatus status = EmergencyManager::instance()->familyStatus(fid);
        switch (status)
        {
        case EffectiveContactStatus::OK:
            symbols += StatusIcons::Checkmark;
            break;
        case EffectiveContactStatus::NeedsHelp:
            symbols += StatusIcons::Flag;
            break;
        case EffectiveContactStatus::UnableToReach:
            symbols += "?";
            break;
        case EffectiveContactStatus::NotContacted:
            symbols += StatusIcons::Circle;
            break;
        }
    }

    return QString("[%1]").arg(symbols);
}

QString MinisteringModel::districtProgressText(TreeNode* districtNode) const
{
    if (!EmergencyManager::instance() || !EmergencyManager::instance()->isActive())
    {
        return QString();
    }

    int contacted = 0;
    int total = 0;

    for (TreeNode* compNode : districtNode->children)
    {
        if (compNode->type != ItemType::Companionship)
        {
            continue;
        }

        TreeNode* ministeredSection = findMinisteredSection(compNode);
        if (!ministeredSection)
        {
            continue;
        }

        for (const TreeNode* child : ministeredSection->children)
        {
            FamilyId fid = familyIdForNode(child);
            if (fid.isNull())
            {
                total++;
                continue;
            }

            total++;
            EffectiveContactStatus status = EmergencyManager::instance()->familyStatus(fid);
            if (status != EffectiveContactStatus::NotContacted)
            {
                contacted++;
            }
        }
    }

    if (total == 0)
    {
        return QString();
    }

    return QString("[%1/%2]").arg(contacted).arg(total);
}

QString MinisteringModel::districtLeaderPhone(const MinisteringDistrict& district) const
{
    if (!EmergencyManager::instance() || !EmergencyManager::instance()->isActive())
    {
        return QString();
    }

    if (!district.hasPresidencyMember())
    {
        return QString();
    }

    const Document& doc = DocumentManager::instance()->document();
    std::optional<Person> leader = doc.findPersonById(*district.presidencyMemberId());
    if (leader && !leader->phone().isEmpty())
    {
        return leader->phone();
    }
    return QString();
}

QString MinisteringModel::formatCompanionshipText(const QString& ministerNames, int filteredCount,
                                                    TreeNode* companionshipNode) const
{
    QString statusSummary = compactStatusSummary(companionshipNode);
    if (statusSummary.isEmpty())
    {
        return QString("%1 (%2)").arg(ministerNames).arg(filteredCount);
    }
    return QString("%1 (%2) %3").arg(ministerNames).arg(filteredCount).arg(statusSummary);
}

QString MinisteringModel::formatDistrictText(const MinisteringDistrict& district, int districtCount,
                                               TreeNode* districtNode) const
{
    QString baseText = QString("%1 (%2 %3)")
        .arg(district.name())
        .arg(districtCount)
        .arg(isEQ() ? tr("families") : tr("sisters"));

    QString progress = districtProgressText(districtNode);
    if (!progress.isEmpty())
    {
        baseText += " " + progress;
    }

    QString leaderPhone = districtLeaderPhone(district);
    if (!leaderPhone.isEmpty())
    {
        baseText += " " + ContactIcons::Phone + leaderPhone;
    }

    return baseText;
}

void MinisteringModel::refreshFamilyDisplayText(const FamilyId& familyId)
{
    const Document& doc = DocumentManager::instance()->document();
    const auto& families = doc.families();

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

    // Walk tree and update affected nodes
    for (int districtRow = 0; districtRow < m_districtNodes.size(); ++districtRow)
    {
        TreeNode* districtNode = m_districtNodes[districtRow];

        for (int compRow = 0; compRow < districtNode->children.size(); ++compRow)
        {
            TreeNode* compNode = districtNode->children[compRow];
            if (compNode->type != ItemType::Companionship)
            {
                continue;
            }

            bool companionshipAffected = false;

            for (int sectionRow = 0; sectionRow < compNode->children.size(); ++sectionRow)
            {
                TreeNode* sectionNode = compNode->children[sectionRow];
                if (sectionNode->type != ItemType::SectionHeader)
                {
                    continue;
                }

                for (int itemRow = 0; itemRow < sectionNode->children.size(); ++itemRow)
                {
                    TreeNode* itemNode = sectionNode->children[itemRow];
                    bool needsUpdate = false;

                    if (itemNode->type == ItemType::Minister
                        || itemNode->type == ItemType::MinisteredSister)
                    {
                        // Person node - check if person is in updated family
                        if (itemNode->personId && personIds.contains(*itemNode->personId))
                        {
                            std::optional<Person> person = doc.findPersonById(*itemNode->personId);
                            if (person)
                            {
                                itemNode->displayText = person->displayName();
                                needsUpdate = true;
                                if (itemNode->type == ItemType::Minister)
                                {
                                    companionshipAffected = true;
                                }
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
                        QModelIndex sectionIndex = createIndex(sectionRow, 0, sectionNode);
                        QModelIndex itemIndex = index(itemRow, 0, sectionIndex);
                        emit dataChanged(itemIndex, itemIndex);
                    }
                }
            }

            // Update companionship text if any minister was affected
            if (companionshipAffected)
            {
                const QHash<MinisteringGroupId, MinisteringGroup>& groups =
                    isEQ() ? doc.eqGroups() : doc.rsGroups();

                if (compNode->groupId && groups.contains(*compNode->groupId))
                {
                    const MinisteringGroup& group = groups[*compNode->groupId];
                    QStringList ministerNames;
                    for (const PersonId& ministerId : group.ministerIds())
                    {
                        std::optional<Person> person = doc.findPersonById(ministerId);
                        if (person)
                        {
                            ministerNames.append(person->displayName());
                        }
                    }
                    int filteredCount = countMinisteredChildren(compNode);
                    compNode->displayText = formatCompanionshipText(
                        ministerNames.join(", "), filteredCount, compNode);

                    QModelIndex districtIndex = createIndex(districtRow, 0, districtNode);
                    QModelIndex compIndex = index(compRow, 0, districtIndex);
                    emit dataChanged(compIndex, compIndex);
                }
            }
        }

        // Update district header with sum of filtered counts from all companionships
        int districtCount = 0;
        for (TreeNode* compNode : districtNode->children)
        {
            if (compNode->type == ItemType::Companionship)
            {
                districtCount += countMinisteredChildren(compNode);
            }
        }

        const auto& districts = isEQ() ? doc.eqDistricts() : doc.rsDistricts();
        if (districtNode->districtId && districts.contains(*districtNode->districtId))
        {
            const MinisteringDistrict& district = districts[*districtNode->districtId];
            districtNode->displayText = formatDistrictText(district, districtCount, districtNode);

            QModelIndex districtIndex = createIndex(districtRow, 0, districtNode);
            emit dataChanged(districtIndex, districtIndex);
        }
    }
}
