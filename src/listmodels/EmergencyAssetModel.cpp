#include "EmergencyAssetModel.h"
#include "ContactIcons.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyAsset.h"
#include "Family.h"
#include "Filter.h"
#include "Person.h"

#include <algorithm>
#include <QSet>

namespace
{

QString formatContactSuffix(const Person& person)
{
    QStringList parts;
    if (!person.phone().isEmpty())
    {
        parts.append(person.phone());
    }
    if (!person.email().isEmpty())
    {
        parts.append(person.email());
    }
    if (parts.isEmpty())
    {
        return QString();
    }
    return QString::fromUtf8(" \u2014 ") + parts.join(QString::fromUtf8(" \u2014 "));
}

}  // namespace

EmergencyAssetModel::EmergencyAssetModel(Filter* filter,
                                                 ResponseArea area,
                                                 QObject* parent)
    : BaseTreeModel(parent)
    , m_filter(filter)
    , m_area(area)
{
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &EmergencyAssetModel::onDocumentChanged);
    if (m_filter)
    {
        connect(m_filter, &Filter::changed, this, &EmergencyAssetModel::rebuild);
    }
    rebuild();
}

EmergencyAssetModel::~EmergencyAssetModel()
{
    clearNodes();
}

void EmergencyAssetModel::clearNodes()
{
    qDeleteAll(m_assetNodes);
    m_assetNodes.clear();
}

void EmergencyAssetModel::onDocumentChanged(const DocumentChange& change)
{
    // Full document reload or asset changes: rebuild
    if (change.action == ChangeAction::Full || change.assetId)
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

void EmergencyAssetModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = DocumentManager::instance()->document();
    QList<EmergencyAsset> assets = doc.emergencyAssetsByArea(m_area);

    // Sort assets by name
    std::sort(assets.begin(), assets.end(),
              [](const EmergencyAsset& a, const EmergencyAsset& b)
              { return a.name().toLower() < b.name().toLower(); });

    for (const EmergencyAsset& asset : assets)
    {
        // Collect and sort people in this asset (with filtering)
        QList<QPair<PersonId, QString>> people;  // (personId, displayName)
        for (const PersonId& personId : asset.personIds())
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (person)
            {
                // Apply filter if set
                if (m_filter && !m_filter->passes(doc, *person))
                {
                    continue;
                }
                people.append({personId, person->displayName() + formatContactSuffix(*person)});
            }
        }
        std::sort(people.begin(), people.end(),
                  [](const QPair<PersonId, QString>& a, const QPair<PersonId, QString>& b)
                  { return a.second.toLower() < b.second.toLower(); });

        // Create asset node with filtered count
        TreeNode* assetNode = new TreeNode();
        assetNode->type = ItemType::Asset;
        assetNode->assetId = asset.id();
        assetNode->displayText = QString("%1 (%2)")
            .arg(asset.name())
            .arg(people.size());

        // Create person nodes as children of asset
        for (const QPair<PersonId, QString>& personData : people)
        {
            TreeNode* personNode = new TreeNode();
            personNode->type = ItemType::Person;
            personNode->personId = personData.first;
            personNode->assetId = asset.id();
            personNode->displayText = personData.second;
            personNode->parent = assetNode;
            assetNode->children.append(personNode);
        }

        m_assetNodes.append(assetNode);
    }

    endResetModel();
}

EmergencyAssetModel::TreeNode* EmergencyAssetModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}

QModelIndex EmergencyAssetModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        // Top-level: asset rows
        if (row >= 0 && row < m_assetNodes.size())
        {
            return createIndex(row, 0, m_assetNodes.at(row));
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

QModelIndex EmergencyAssetModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // If parent is an asset node (top-level)
    int assetRow = m_assetNodes.indexOf(parentNode);
    if (assetRow >= 0)
    {
        return createIndex(assetRow, 0, parentNode);
    }

    // Parent is a person node - find its row within the asset
    if (parentNode->parent)
    {
        int personRow = parentNode->parent->children.indexOf(parentNode);
        if (personRow >= 0)
        {
            return createIndex(personRow, 0, parentNode);
        }
    }

    return QModelIndex();
}

int EmergencyAssetModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        // Root: number of assets
        return m_assetNodes.size();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return node->children.size();
    }

    return 0;
}

int EmergencyAssetModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool EmergencyAssetModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_assetNodes.isEmpty();
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

QVariant EmergencyAssetModel::data(const QModelIndex& index, int role) const
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
    case ItemTypeRole:
        return QVariant::fromValue(node->type);
    case AssetIdRole:
        return node->assetId.toString();
    default:
        return QVariant();
    }
}

SelectionKey EmergencyAssetModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return SelectionKey::literal(QString());
    }

    switch (node->type)
    {
    case ItemType::Asset:
        return SelectionKey::from(node->assetId);
    case ItemType::Person:
        if (node->personId)
        {
            return SelectionKey::literal(node->assetId.toString() + ":" + node->personId->toString());
        }
        return SelectionKey::from(node->assetId);
    case ItemType::ContactDetail:
        return selectionKeyAt(index.parent());
    default:
        return SelectionKey::literal(QString());
    }
}

FamilyAssociation EmergencyAssetModel::relatedFamiliesAt(const QModelIndex& index) const
{
    FamilyAssociation assoc;
    if (!index.isValid())
    {
        return assoc;
    }

    const Document& doc = DocumentManager::instance()->document();
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return assoc;
    }

    switch (node->type)
    {
    case ItemType::Asset:
    {
        std::optional<EmergencyAsset> assetOpt = doc.findEmergencyAssetById(node->assetId);
        if (assetOpt)
        {
            for (const PersonId& personId : assetOpt->personIds())
            {
                std::optional<FamilyId> familyId = doc.familyIdForPerson(personId);
                if (familyId)
                {
                    assoc.relatedFamilyIds.insert(*familyId);
                }
            }
        }
        break;
    }
    case ItemType::Person:
    {
        if (node->personId)
        {
            std::optional<FamilyId> familyId = doc.familyIdForPerson(*node->personId);
            if (familyId)
            {
                assoc.relatedFamilyIds.insert(*familyId);
            }
        }
        break;
    }
    default:
        break;
    }

    return assoc;
}

ItemType EmergencyAssetModel::itemTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return ItemType::Invalid;
}

std::optional<EmergencyAssetId> EmergencyAssetModel::assetIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return std::nullopt;
    }
    return node->assetId;
}

QModelIndex EmergencyAssetModel::indexForFamilyId(const FamilyId& familyId) const
{
    const Document& doc = DocumentManager::instance()->document();

    for (int a = 0; a < m_assetNodes.size(); ++a)
    {
        TreeNode* assetNode = m_assetNodes[a];
        for (int p = 0; p < assetNode->children.size(); ++p)
        {
            TreeNode* personNode = assetNode->children[p];
            if (personNode->type == ItemType::Person && personNode->personId)
            {
                std::optional<FamilyId> fid = doc.familyIdForPerson(*personNode->personId);
                if (fid && *fid == familyId)
                {
                    return createIndex(p, 0, personNode);
                }
            }
        }
    }
    return {};
}

std::optional<PersonId> EmergencyAssetModel::personIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return std::nullopt;
    }
    return node->personId;
}

void EmergencyAssetModel::loadContactDetails(const QModelIndex& index)
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->contactsLoaded)
    {
        return;
    }

    // Only load contacts for person nodes
    if (node->type != ItemType::Person)
    {
        return;
    }

    const Document& doc = DocumentManager::instance()->document();
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

    int insertRow = node->children.size();
    beginInsertRows(index, insertRow, insertRow + itemCount - 1);

    // Phone
    if (!person->phone().isEmpty())
    {
        TreeNode* phoneNode = new TreeNode();
        phoneNode->type = ItemType::ContactDetail;
        phoneNode->personId = node->personId;
        phoneNode->assetId = node->assetId;
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
        altPhoneNode->assetId = node->assetId;
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
        emailNode->assetId = node->assetId;
        emailNode->displayText = ContactIcons::Email + person->email();
        emailNode->parent = node;
        node->children.append(emailNode);
    }

    // Address (from family)
    if (hasAddress)
    {
        const Family& family = families[*familyId];
        TreeNode* addrNode = new TreeNode();
        addrNode->type = ItemType::ContactDetail;
        addrNode->personId = node->personId;
        addrNode->assetId = node->assetId;
        addrNode->displayText = ContactIcons::Address + family.address().full();
        addrNode->parent = node;
        node->children.append(addrNode);
    }

    endInsertRows();
    node->contactsLoaded = true;
}

void EmergencyAssetModel::refreshFamilyDisplayText(const FamilyId& familyId)
{
    const Document& doc = DocumentManager::instance()->document();
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

    // Walk tree and update affected person nodes
    for (int assetRow = 0; assetRow < m_assetNodes.size(); ++assetRow)
    {
        TreeNode* assetNode = m_assetNodes[assetRow];

        for (int personRow = 0; personRow < assetNode->children.size(); ++personRow)
        {
            TreeNode* personNode = assetNode->children[personRow];

            if (personNode->type == ItemType::Person
                && personNode->personId
                && personIds.contains(*personNode->personId))
            {
                std::optional<Person> person = doc.findPersonById(*personNode->personId);
                if (person)
                {
                    personNode->displayText = person->displayName() + formatContactSuffix(*person);

                    QModelIndex assetIndex = createIndex(assetRow, 0, assetNode);
                    QModelIndex personIndex = index(personRow, 0, assetIndex);
                    emit dataChanged(personIndex, personIndex);
                }
            }
        }
    }
}
