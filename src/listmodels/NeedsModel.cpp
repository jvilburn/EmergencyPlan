#include "NeedsModel.h"
#include "ContactIcons.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Family.h"
#include "Filter.h"
#include "Person.h"

#include <algorithm>

NeedsModel::NeedsModel(Filter* filter,
                       QObject* parent)
    : BaseTreeModel(parent)
    , m_filter(filter)
{
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &NeedsModel::onDocumentChanged);
    if (m_filter)
    {
        connect(m_filter, &Filter::changed, this, &NeedsModel::rebuild);
    }
    rebuild();
}

NeedsModel::~NeedsModel()
{
    clearNodes();
}

void NeedsModel::clearNodes()
{
    qDeleteAll(m_personNodes);
    m_personNodes.clear();
}

NeedsModel::TreeNode* NeedsModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}

bool NeedsModel::shouldRebuild(const DocumentChange& change) const
{
    return change.action == ChangeAction::Full || change.familyId.has_value();
}

void NeedsModel::onDocumentChanged(const DocumentChange& change)
{
    if (shouldRebuild(change))
    {
        rebuild();
    }
}

void NeedsModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = DocumentManager::instance()->document();

    // Local struct for sorting - avoids storing sortKey in TreeNode after sort
    struct SortEntry
    {
        TreeNode* node;
        QString sortKey;
    };

    // Collect all persons with special needs
    QList<SortEntry> unsorted;
    for (const Family& family : doc.families())
    {
        for (const Person& person : family.members())
        {
            if (person.hasSpecialNeed())
            {
                // Apply filter if set
                if (m_filter && !m_filter->passes(doc, person))
                {
                    continue;
                }

                TreeNode* node = new TreeNode();
                node->type = ItemType::Person;
                node->personId = person.id();
                node->familyId = family.id();

                QString text = person.displayName() + formatContactSuffix(person);
                if (!person.specialNeedNote().isEmpty())
                {
                    text += QString(" - %1").arg(person.specialNeedNote());
                }
                node->displayText = text;

                unsorted.append({node, person.displayName().toLower()});
            }
        }
    }

    // Sort alphabetically by display name
    std::sort(unsorted.begin(), unsorted.end(),
              [](const SortEntry& a, const SortEntry& b) { return a.sortKey < b.sortKey; });

    // Extract just the nodes
    for (const SortEntry& entry : unsorted)
    {
        m_personNodes.append(entry.node);
    }

    endResetModel();
}

QModelIndex NeedsModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        // Top-level: person rows
        if (row >= 0 && row < m_personNodes.size())
        {
            return createIndex(row, 0, m_personNodes.at(row));
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

QModelIndex NeedsModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // Parent is always a person node (top-level)
    int row = m_personNodes.indexOf(parentNode);
    if (row >= 0)
    {
        return createIndex(row, 0, parentNode);
    }

    return QModelIndex();
}

int NeedsModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return m_personNodes.size();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return node->children.size();
    }

    return 0;
}

int NeedsModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool NeedsModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_personNodes.isEmpty();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (!node)
    {
        return false;
    }

    // Person nodes can have contact children (lazy loaded)
    if (node->type == ItemType::Person)
    {
        return true;
    }

    return !node->children.isEmpty();
}

QVariant NeedsModel::data(const QModelIndex& index, int role) const
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
    case PersonIdRole:
        return node->personId.toString();
    case FamilyIdRole:
        return node->familyId.toString();
    default:
        return QVariant();
    }
}

SelectionKey NeedsModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return SelectionKey::literal(QString());
    }

    switch (node->type)
    {
    case ItemType::Person:
        return SelectionKey::from(node->personId);
    case ItemType::ContactDetail:
        return selectionKeyAt(index.parent());
    default:
        return SelectionKey::literal(QString());
    }
}

FamilyAssociation NeedsModel::relatedFamiliesAt(const QModelIndex& index) const
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

    if (node->type == ItemType::Person || node->type == ItemType::ContactDetail)
    {
        const TreeNode* personNode = (node->type == ItemType::ContactDetail && node->parent)
                                         ? node->parent
                                         : node;
        const Document& doc = DocumentManager::instance()->document();
        std::optional<FamilyId> familyId = doc.familyIdForPerson(personNode->personId);
        if (familyId)
        {
            assoc.relatedFamilyIds.insert(*familyId);
        }
    }

    return assoc;
}

std::optional<PersonId> NeedsModel::personIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return std::nullopt;
    }
    return node->personId;
}

std::optional<FamilyId> NeedsModel::familyIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return std::nullopt;
    }
    return node->familyId;
}

QModelIndex NeedsModel::indexForPersonId(const PersonId& personId) const
{
    for (int i = 0; i < m_personNodes.size(); ++i)
    {
        if (m_personNodes.at(i)->personId == personId)
        {
            return createIndex(i, 0, m_personNodes.at(i));
        }
    }
    return {};
}

QModelIndex NeedsModel::indexForFamilyId(const FamilyId& familyId) const
{
    for (int i = 0; i < m_personNodes.size(); ++i)
    {
        if (m_personNodes.at(i)->familyId == familyId)
        {
            return createIndex(i, 0, m_personNodes.at(i));
        }
    }
    return {};
}

ItemType NeedsModel::itemTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return ItemType::Invalid;
}

void NeedsModel::loadContactDetails(const QModelIndex& index)
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->contactsLoaded)
    {
        return;
    }

    if (node->type != ItemType::Person)
    {
        return;
    }

    const Document& doc = DocumentManager::instance()->document();
    std::optional<Person> person = doc.findPersonById(node->personId);
    if (!person)
    {
        node->contactsLoaded = true;
        return;
    }

    // Check for address availability
    const QHash<FamilyId, Family>& families = doc.families();
    bool hasAddress = false;
    if (families.contains(node->familyId))
    {
        hasAddress = !families[node->familyId].address().isEmpty();
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

    int insertRow = node->children.size();
    beginInsertRows(index, insertRow, insertRow + itemCount - 1);

    // Phone
    if (!person->phone().isEmpty())
    {
        TreeNode* phoneNode = new TreeNode();
        phoneNode->type = ItemType::ContactDetail;
        phoneNode->personId = node->personId;
        phoneNode->familyId = node->familyId;
        phoneNode->displayText = ContactIcons::Phone + person->phone();
        phoneNode->parent = node;
        node->children.append(phoneNode);
    }

    // Alt phone
    if (!person->altPhone().isEmpty())
    {
        TreeNode* altPhoneNode = new TreeNode();
        altPhoneNode->type = ItemType::ContactDetail;
        altPhoneNode->personId = node->personId;
        altPhoneNode->familyId = node->familyId;
        altPhoneNode->displayText = ContactIcons::Phone + person->altPhone() + tr(" (alt)");
        altPhoneNode->parent = node;
        node->children.append(altPhoneNode);
    }

    // Email
    if (!person->email().isEmpty())
    {
        TreeNode* emailNode = new TreeNode();
        emailNode->type = ItemType::ContactDetail;
        emailNode->personId = node->personId;
        emailNode->familyId = node->familyId;
        emailNode->displayText = ContactIcons::Email + person->email();
        emailNode->parent = node;
        node->children.append(emailNode);
    }

    // Address (from family)
    if (hasAddress)
    {
        const Family& family = families[node->familyId];
        TreeNode* addrNode = new TreeNode();
        addrNode->type = ItemType::ContactDetail;
        addrNode->personId = node->personId;
        addrNode->familyId = node->familyId;
        addrNode->displayText = ContactIcons::Address + family.address().full();
        addrNode->parent = node;
        node->children.append(addrNode);
    }

    endInsertRows();
    node->contactsLoaded = true;
}
