#include "MinisteringModel.h"
#include "DocumentManager.h"
#include "Document.h"
#include "MinisteringDistrict.h"
#include "MinisteringGroup.h"
#include "Family.h"
#include "Person.h"

#include <algorithm>

MinisteringModel::MinisteringModel(DocumentManager* documentManager,
                                     bool isEQ,
                                     QObject* parent)
    : QAbstractItemModel(parent)
    , m_documentManager(documentManager)
    , m_isEQ(isEQ)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &MinisteringModel::onDocumentChanged);
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

bool MinisteringModel::shouldRebuild(const DocumentChange& change) const
{
    switch (change.scope)
    {
    case ChangeScope::Full:
    case ChangeScope::EqDistrict:
    case ChangeScope::EqGroup:
    case ChangeScope::RsDistrict:
    case ChangeScope::RsGroup:
    case ChangeScope::Family:  // Person names and contact info are in families
        return true;
    default:
        return false;
    }
}

void MinisteringModel::onDocumentChanged(const DocumentChange& change)
{
    if (shouldRebuild(change))
    {
        rebuild();
    }
}

void MinisteringModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = m_documentManager->document();
    const QHash<QString, MinisteringDistrict>& districts = m_isEQ ? doc.eqDistricts() : doc.rsDistricts();
    const QHash<QString, MinisteringGroup>& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();

    // Sort districts by name
    QList<MinisteringDistrict> sortedDistricts = districts.values();
    std::sort(sortedDistricts.begin(), sortedDistricts.end(),
              [](const MinisteringDistrict& a, const MinisteringDistrict& b)
              { return a.name().toLower() < b.name().toLower(); });

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

        TreeNode* districtNode = new TreeNode();
        districtNode->type = NodeType::District;
        districtNode->id = district.id();
        districtNode->displayText = districtText;

        // Collect and sort companionships by minister names
        QList<QPair<QString, QString>> sortedGroups;  // (display text, group ID)
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

            int count = m_isEQ ? group.familyCount() : group.ministeredPersonCount();
            QString companionshipText = QString("%1 (%2)")
                .arg(ministerNames.join(", "))
                .arg(count);

            sortedGroups.append({companionshipText, groupId});
        }

        std::sort(sortedGroups.begin(), sortedGroups.end(),
                  [](const QPair<QString, QString>& a, const QPair<QString, QString>& b)
                  { return a.first.toLower() < b.first.toLower(); });

        // Add companionships under this district
        for (const QPair<QString, QString>& groupData : sortedGroups)
        {
            const QString& companionshipText = groupData.first;
            const QString& groupId = groupData.second;

            TreeNode* companionshipNode = new TreeNode();
            companionshipNode->type = NodeType::Companionship;
            companionshipNode->id = groupId;
            companionshipNode->displayText = companionshipText;
            companionshipNode->parent = districtNode;
            districtNode->children.append(companionshipNode);

            // Add ministers and ministered sections
            addMinistersSection(companionshipNode, groupId);
            addMinisteredSection(companionshipNode, groupId);
        }

        m_districtNodes.append(districtNode);
    }

    endResetModel();
}

void MinisteringModel::addMinistersSection(TreeNode* companionshipNode, const QString& groupId)
{
    const Document& doc = m_documentManager->document();
    const QHash<QString, MinisteringGroup>& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();

    if (!groups.contains(groupId))
    {
        return;
    }

    const MinisteringGroup& group = groups[groupId];

    // Create "Ministers" section header
    TreeNode* ministersHeader = new TreeNode();
    ministersHeader->type = NodeType::SectionHeader;
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
        ministerNode->type = NodeType::Minister;
        ministerNode->id = ministerData.second;
        ministerNode->displayText = ministerData.first;
        ministerNode->parent = ministersHeader;
        ministersHeader->children.append(ministerNode);
    }
}

void MinisteringModel::addMinisteredSection(TreeNode* companionshipNode, const QString& groupId)
{
    const Document& doc = m_documentManager->document();
    const QHash<QString, MinisteringGroup>& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();

    if (!groups.contains(groupId))
    {
        return;
    }

    const MinisteringGroup& group = groups[groupId];

    // Create section header - "Families" for EQ, "Sisters" for RS
    TreeNode* ministeredHeader = new TreeNode();
    ministeredHeader->type = NodeType::SectionHeader;
    ministeredHeader->id = groupId + ":ministered";
    ministeredHeader->displayText = m_isEQ ? tr("Families") : tr("Sisters");
    ministeredHeader->parent = companionshipNode;
    companionshipNode->children.append(ministeredHeader);

    if (m_isEQ)
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
                sortedFamilies.append({family.displayName(), familyId});
            }
        }

        std::sort(sortedFamilies.begin(), sortedFamilies.end(),
                  [](const QPair<QString, QString>& a, const QPair<QString, QString>& b)
                  { return a.first.toLower() < b.first.toLower(); });

        for (const QPair<QString, QString>& familyData : sortedFamilies)
        {
            TreeNode* familyNode = new TreeNode();
            familyNode->type = NodeType::MinisteredFamily;
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
                sortedSisters.append({person->displayName(), personId});
            }
        }

        std::sort(sortedSisters.begin(), sortedSisters.end(),
                  [](const QPair<QString, QString>& a, const QPair<QString, QString>& b)
                  { return a.first.toLower() < b.first.toLower(); });

        for (const QPair<QString, QString>& sisterData : sortedSisters)
        {
            TreeNode* sisterNode = new TreeNode();
            sisterNode->type = NodeType::MinisteredSister;
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
            if (node->type == NodeType::SectionHeader)
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
            if (node->type == NodeType::SectionHeader)
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

MinisteringModel::NodeType MinisteringModel::nodeTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return NodeType::District;
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
        if (node->type == NodeType::Companionship)
        {
            return node->id;
        }
        node = node->parent;
    }
    return QString();
}

void MinisteringModel::loadContactDetails(const QModelIndex& index)
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->contactsLoaded)
    {
        return;
    }

    // Only load contacts for person/family nodes
    if (node->type != NodeType::Minister
        && node->type != NodeType::MinisteredFamily
        && node->type != NodeType::MinisteredSister)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    int insertRow = node->children.size();

    if (node->type == NodeType::Minister || node->type == NodeType::MinisteredSister)
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
            phoneNode->type = NodeType::ContactDetail;
            phoneNode->displayText = QString::fromUtf8("\xf0\x9f\x93\x9e ") + person->phone();
            phoneNode->secondaryId = node->id;
            phoneNode->parent = node;
            node->children.append(phoneNode);
        }

        // Alt phone
        if (!person->altPhone().isEmpty())
        {
            TreeNode* altPhoneNode = new TreeNode();
            altPhoneNode->type = NodeType::ContactDetail;
            altPhoneNode->displayText = QString::fromUtf8("\xf0\x9f\x93\x9e ") + person->altPhone() + tr(" (alt)");
            altPhoneNode->secondaryId = node->id;
            altPhoneNode->parent = node;
            node->children.append(altPhoneNode);
        }

        // Email
        if (!person->email().isEmpty())
        {
            TreeNode* emailNode = new TreeNode();
            emailNode->type = NodeType::ContactDetail;
            emailNode->displayText = QString::fromUtf8("\xf0\x9f\x93\xa7 ") + person->email();
            emailNode->secondaryId = node->id;
            emailNode->parent = node;
            node->children.append(emailNode);
        }

        // Address (from family)
        if (hasAddress)
        {
            const Family& family = families[familyId];
            TreeNode* addrNode = new TreeNode();
            addrNode->type = NodeType::ContactDetail;
            addrNode->displayText = QString::fromUtf8("\xf0\x9f\x93\x8d ") + family.address().full();
            addrNode->secondaryId = node->id;
            addrNode->parent = node;
            node->children.append(addrNode);
        }

        endInsertRows();
    }
    else if (node->type == NodeType::MinisteredFamily)
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
                    phoneNode->type = NodeType::ContactDetail;
                    phoneNode->displayText = QString::fromUtf8("\xf0\x9f\x93\x9e ") + member.phone();
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
            addrNode->type = NodeType::ContactDetail;
            addrNode->displayText = QString::fromUtf8("\xf0\x9f\x93\x8d ") + family.address().full();
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
