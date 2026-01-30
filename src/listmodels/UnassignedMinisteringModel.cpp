#include "UnassignedMinisteringModel.h"
#include "DocumentManager.h"
#include "Document.h"
#include "MinisteringGroup.h"
#include "Family.h"
#include "Person.h"

#include <algorithm>

UnassignedMinisteringModel::UnassignedMinisteringModel(DocumentManager* documentManager,
                                                         bool isEQ,
                                                         QObject* parent)
    : QAbstractItemModel(parent)
    , m_documentManager(documentManager)
    , m_isEQ(isEQ)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &UnassignedMinisteringModel::onDocumentChanged);
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

bool UnassignedMinisteringModel::shouldRebuild(const DocumentChange& change) const
{
    switch (change.scope)
    {
    case ChangeScope::Full:
    case ChangeScope::EqGroup:
    case ChangeScope::RsGroup:
    case ChangeScope::Family:
        return true;
    default:
        return false;
    }
}

void UnassignedMinisteringModel::onDocumentChanged(const DocumentChange& change)
{
    if (shouldRebuild(change))
    {
        rebuild();
    }
}

void UnassignedMinisteringModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = m_documentManager->document();
    const QHash<QString, MinisteringGroup>& groups = m_isEQ ? doc.eqGroups() : doc.rsGroups();

    if (m_isEQ)
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
            m_headerNode->type = NodeType::UnassignedHeader;
            m_headerNode->id = "unassigned";
            m_headerNode->displayText = tr("Unassigned (%1 families)").arg(unassignedIds.size());

            // Sort families by name
            QList<QPair<QString, QString>> sortedFamilies;
            for (const QString& familyId : unassignedIds)
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
            m_headerNode->type = NodeType::UnassignedHeader;
            m_headerNode->id = "unassigned";
            m_headerNode->displayText = tr("Unassigned (%1 sisters)").arg(unassignedIds.size());

            // Sort sisters by name
            QList<QPair<QString, QString>> sortedSisters;
            for (const QString& personId : unassignedIds)
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

UnassignedMinisteringModel::NodeType UnassignedMinisteringModel::nodeTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return NodeType::Invalid;
}

void UnassignedMinisteringModel::loadContactDetails(const QModelIndex& index)
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->contactsLoaded)
    {
        return;
    }

    // Only load contacts for person/family nodes
    if (node->type != NodeType::MinisteredFamily && node->type != NodeType::MinisteredSister)
    {
        return;
    }

    const Document& doc = m_documentManager->document();
    int insertRow = node->children.size();

    if (node->type == NodeType::MinisteredSister)
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
