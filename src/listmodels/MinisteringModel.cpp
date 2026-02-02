#include "MinisteringModel.h"
#include "ContactIcons.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Filter.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "Family.h"
#include "Person.h"

#include <algorithm>
#include <QFont>

MinisteringModel::MinisteringModel(DocumentManager* documentManager,
                                     Filter* filter,
                                     MinisteringOrg org,
                                     QObject* parent)
    : BaseTreeModel(parent)
    , m_documentManager(documentManager)
    , m_filter(filter)
    , m_org(org)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &MinisteringModel::onDocumentChanged);
    if (m_filter)
    {
        connect(m_filter, &Filter::changed, this, &MinisteringModel::rebuild);
    }
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
    // Structural changes: full rebuild
    if (change.scope == ChangeScope::Full
        || change.action == ChangeAction::BatchModified)
    {
        rebuild();
        return;
    }

    // Ministering structure changes: full rebuild
    if (change.scope == ChangeScope::EqDistrict
        || change.scope == ChangeScope::EqGroup
        || change.scope == ChangeScope::RsDistrict
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

    // Family added/removed: rebuild (rare, structure might change)
    if (change.scope == ChangeScope::Family)
    {
        rebuild();
    }
}

void MinisteringModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = m_documentManager->document();
    const QHash<QString, MinisteringDistrict>& districts = isEQ() ? doc.eqDistricts() : doc.rsDistricts();
    const QHash<QString, MinisteringGroup>& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    // Sort districts by name
    QList<MinisteringDistrict> sortedDistricts = districts.values();
    std::sort(sortedDistricts.begin(), sortedDistricts.end(),
              [](const MinisteringDistrict& a, const MinisteringDistrict& b)
              { return a.name().toLower() < b.name().toLower(); });

    for (const MinisteringDistrict& district : sortedDistricts)
    {
        TreeNode* districtNode = new TreeNode();
        districtNode->type = ItemType::District;
        districtNode->id = district.id();
        // Display text will be updated after filtering

        // Collect companionships with minister names for sorting
        QList<QPair<QString, QString>> sortedGroups;  // (minister names, group ID)
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
                std::optional<Person> person = doc.findPersonById(ministerId);
                if (person)
                {
                    ministerNames.append(person->displayName());
                }
            }

            sortedGroups.append({ministerNames.join(", "), groupId});
        }

        std::sort(sortedGroups.begin(), sortedGroups.end(),
                  [](const QPair<QString, QString>& a, const QPair<QString, QString>& b)
                  { return a.first.toLower() < b.first.toLower(); });

        // Add companionships under this district
        int districtCount = 0;
        for (const QPair<QString, QString>& groupData : sortedGroups)
        {
            const QString& ministerNames = groupData.first;
            const QString& groupId = groupData.second;

            TreeNode* companionshipNode = new TreeNode();
            companionshipNode->type = ItemType::Companionship;
            companionshipNode->id = groupId;
            // Display text will be updated after filtering
            companionshipNode->parent = districtNode;
            districtNode->children.append(companionshipNode);

            // Add ministers and ministered sections
            addMinistersSection(companionshipNode, groupId);
            addMinisteredSection(companionshipNode, groupId);

            // Count filtered items and update companionship text
            int filteredCount = countMinisteredChildren(companionshipNode);
            companionshipNode->displayText = QString("%1 (%2)")
                .arg(ministerNames)
                .arg(filteredCount);

            districtCount += filteredCount;
        }

        // Update district text with filtered count
        districtNode->displayText = QString("%1 (%2 %3)")
            .arg(district.name())
            .arg(districtCount)
            .arg(isEQ() ? tr("families") : tr("sisters"));

        m_districtNodes.append(districtNode);
    }

    endResetModel();
}

void MinisteringModel::addMinistersSection(TreeNode* companionshipNode, const QString& groupId)
{
    const Document& doc = m_documentManager->document();
    const QHash<QString, MinisteringGroup>& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    if (!groups.contains(groupId))
    {
        return;
    }

    const MinisteringGroup& group = groups[groupId];

    // Create "Ministers" section header
    TreeNode* ministersHeader = new TreeNode();
    ministersHeader->type = ItemType::SectionHeader;
    ministersHeader->id = groupId + ":ministers";
    ministersHeader->displayText = tr("Ministers");
    ministersHeader->parent = companionshipNode;
    companionshipNode->children.append(ministersHeader);

    // Collect and sort ministers by name
    QList<QPair<QString, QString>> sortedMinisters;  // (display name, person ID)
    for (const QString& ministerId : group.ministerIds())
    {
        std::optional<Person> person = doc.findPersonById(ministerId);
        if (person)
        {
            sortedMinisters.append({person->displayName(), ministerId});
        }
    }

    std::sort(sortedMinisters.begin(), sortedMinisters.end(),
              [](const QPair<QString, QString>& a, const QPair<QString, QString>& b)
              { return a.first.toLower() < b.first.toLower(); });

    // Add individual ministers
    for (const QPair<QString, QString>& ministerData : sortedMinisters)
    {
        TreeNode* ministerNode = new TreeNode();
        ministerNode->type = ItemType::Minister;
        ministerNode->id = ministerData.second;
        ministerNode->displayText = ministerData.first;
        ministerNode->parent = ministersHeader;
        ministersHeader->children.append(ministerNode);
    }
}

void MinisteringModel::addMinisteredSection(TreeNode* companionshipNode, const QString& groupId)
{
    const Document& doc = m_documentManager->document();
    const QHash<QString, MinisteringGroup>& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    if (!groups.contains(groupId))
    {
        return;
    }

    const MinisteringGroup& group = groups[groupId];

    // Create section header - "Families" for EQ, "Sisters" for RS
    TreeNode* ministeredHeader = new TreeNode();
    ministeredHeader->type = ItemType::SectionHeader;
    ministeredHeader->id = groupId + ":ministered";
    ministeredHeader->displayText = isEQ() ? tr("Families") : tr("Sisters");
    ministeredHeader->parent = companionshipNode;
    companionshipNode->children.append(ministeredHeader);

    if (isEQ())
    {
        // EQ: Add families
        const QHash<QString, Family>& families = doc.families();

        // Collect and sort families by name
        QList<QPair<QString, QString>> sortedFamilies;  // (display name, family ID)
        for (const QString& familyId : group.familyIds())
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
            familyNode->parent = ministeredHeader;
            ministeredHeader->children.append(familyNode);
        }
    }
    else
    {
        // RS: Add sisters
        QList<QPair<QString, QString>> sortedSisters;  // (display name, person ID)
        for (const QString& personId : group.ministeredPersonIds())
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

QString MinisteringModel::idAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->id;
    }
    return QString();
}

QString MinisteringModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    switch (node->type)
    {
    case ItemType::District:
        return node->id;
    case ItemType::Companionship:
        return node->id;  // groupId stored in node->id
    case ItemType::SectionHeader:
        return node->id;  // Already formatted as "{groupId}:ministers" etc.
    case ItemType::Minister:
        return QString("%1:minister:%2").arg(companionshipIdAt(index), node->id);
    case ItemType::MinisteredFamily:
        return QString("%1:family:%2").arg(companionshipIdAt(index), node->id);
    case ItemType::MinisteredSister:
        return QString("%1:sister:%2").arg(companionshipIdAt(index), node->id);
    case ItemType::ContactDetail:
        // Delegate to parent (selecting detail row selects parent)
        return selectionKeyAt(index.parent());
    default:
        return QString();
    }
}

QSet<QString> MinisteringModel::familyIdsForPersons(const QSet<QString>& personIds) const
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

int MinisteringModel::countMinisteredChildren(TreeNode* companionshipNode) const
{
    for (TreeNode* section : companionshipNode->children)
    {
        if (section->type == ItemType::SectionHeader
            && section->id.endsWith(":ministered"))
        {
            return section->children.size();
        }
    }
    return 0;
}

FamilyAssociation MinisteringModel::relatedFamiliesAt(const QModelIndex& index) const
{
    FamilyAssociation assoc;
    if (!index.isValid())
    {
        return assoc;
    }

    ItemType type = itemTypeAt(index);
    QString id = idAt(index);

    const Document& doc = m_documentManager->document();
    const auto& districts = isEQ() ? doc.eqDistricts() : doc.rsDistricts();
    const auto& groups = isEQ() ? doc.eqGroups() : doc.rsGroups();

    switch (type)
    {
    case ItemType::District:
        if (districts.contains(id))
        {
            const MinisteringDistrict& district = districts[id];
            for (const QString& groupId : district.groupIds())
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
        if (groups.contains(id))
        {
            const MinisteringGroup& group = groups[id];
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
        int colonPos = id.lastIndexOf(':');
        if (colonPos > 0)
        {
            QString compId = id.left(colonPos);
            QString sectionType = id.mid(colonPos + 1);

            if (groups.contains(compId))
            {
                const MinisteringGroup& group = groups[compId];
                if (sectionType == "ministers")
                {
                    QSet<QString> ministerFamilies = familyIdsForPersons(group.ministerIds());
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
        }
        break;
    }

    case ItemType::Minister:
    {
        QString familyId = doc.familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            assoc.relatedFamilyIds.insert(familyId);
            assoc.contactPointFamilyIds.insert(familyId);
        }
        break;
    }

    case ItemType::MinisteredFamily:
        assoc.relatedFamilyIds.insert(id);
        break;

    case ItemType::MinisteredSister:
    {
        QString familyId = doc.familyIdForPerson(id);
        if (!familyId.isEmpty())
        {
            assoc.relatedFamilyIds.insert(familyId);
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

QString MinisteringModel::companionshipIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    // Walk up to find the companionship
    while (node)
    {
        if (node->type == ItemType::Companionship)
        {
            return node->id;
        }
        node = node->parent;
    }
    return QString();
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

    const Document& doc = m_documentManager->document();
    int insertRow = node->children.size();

    if (node->type == ItemType::Minister || node->type == ItemType::MinisteredSister)
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

bool MinisteringModel::hasContactsLoaded(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->contactsLoaded;
    }
    return false;
}

void MinisteringModel::refreshFamilyDisplayText(const QString& familyId)
{
    const Document& doc = m_documentManager->document();
    const auto& families = doc.families();

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
                        if (personIds.contains(itemNode->id))
                        {
                            std::optional<Person> person = doc.findPersonById(itemNode->id);
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
                        if (itemNode->id == familyId)
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
                // Rebuild companionship display text from current minister names
                const QHash<QString, MinisteringGroup>& groups =
                    isEQ() ? doc.eqGroups() : doc.rsGroups();

                if (groups.contains(compNode->id))
                {
                    const MinisteringGroup& group = groups[compNode->id];
                    QStringList ministerNames;
                    for (const QString& ministerId : group.ministerIds())
                    {
                        std::optional<Person> person = doc.findPersonById(ministerId);
                        if (person)
                        {
                            ministerNames.append(person->displayName());
                        }
                    }
                    // Use filtered count from actual children
                    int filteredCount = countMinisteredChildren(compNode);
                    compNode->displayText = QString("%1 (%2)")
                        .arg(ministerNames.join(", "))
                        .arg(filteredCount);

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
        if (districts.contains(districtNode->id))
        {
            districtNode->displayText = QString("%1 (%2 %3)")
                .arg(districts[districtNode->id].name())
                .arg(districtCount)
                .arg(isEQ() ? tr("families") : tr("sisters"));

            QModelIndex districtIndex = createIndex(districtRow, 0, districtNode);
            emit dataChanged(districtIndex, districtIndex);
        }
    }
}
