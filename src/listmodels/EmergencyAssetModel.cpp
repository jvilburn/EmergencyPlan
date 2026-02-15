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

EmergencyAssetModel::EmergencyAssetModel(DocumentManager* documentManager,
                                                 Filter* filter,
                                                 ResponseArea area,
                                                 QObject* parent)
    : BaseTreeModel(parent)
    , m_documentManager(documentManager)
    , m_filter(filter)
    , m_area(area)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
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
    // Structural changes: full rebuild
    if (change.scope == ChangeScope::Full
        || change.action == ChangeAction::BatchModified
        || change.scope == ChangeScope::EmergencyAsset)
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

    // Family added/removed: rebuild
    if (change.scope == ChangeScope::Family)
    {
        rebuild();
    }
}

void EmergencyAssetModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = m_documentManager->document();
    QList<EmergencyAsset> assets = doc.emergencyAssetsByArea(m_area);

    // Sort assets by name
    std::sort(assets.begin(), assets.end(),
              [](const EmergencyAsset& a, const EmergencyAsset& b)
              { return a.name().toLower() < b.name().toLower(); });

    for (const EmergencyAsset& asset : assets)
    {
        // Collect and sort people in this asset (with filtering)
        QList<QPair<QString, QString>> people;  // (personId, displayName)
        for (const QString& personId : asset.personIds())
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (person)
            {
                // Apply filter if set
                if (m_filter && !m_filter->passes(doc, *person))
                {
                    continue;
                }
                people.append({personId, person->displayName()});
            }
        }
        std::sort(people.begin(), people.end(),
                  [](const QPair<QString, QString>& a, const QPair<QString, QString>& b)
                  { return a.second.toLower() < b.second.toLower(); });

        // Create asset node with filtered count
        TreeNode* assetNode = new TreeNode();
        assetNode->type = ItemType::Asset;
        assetNode->id = asset.id();
        assetNode->displayText = QString("%1 (%2)")
            .arg(asset.name())
            .arg(people.size());

        // Create person nodes as children of asset
        for (const QPair<QString, QString>& personData : people)
        {
            TreeNode* personNode = new TreeNode();
            personNode->type = ItemType::Person;
            personNode->id = personData.first;
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
    case IdRole:
        return node->id;
    case ItemTypeRole:
        return QVariant::fromValue(node->type);
    case AssetIdRole:
        return node->assetId;
    default:
        return QVariant();
    }
}

QString EmergencyAssetModel::idAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->id;
    }
    return QString();
}

QString EmergencyAssetModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    switch (node->type)
    {
    case ItemType::Asset:
        return node->id;
    case ItemType::Person:
        return QString("%1:%2").arg(node->assetId, node->id);
    case ItemType::ContactDetail:
        // Delegate to parent
        return selectionKeyAt(index.parent());
    default:
        return QString();
    }
}

FamilyAssociation EmergencyAssetModel::relatedFamiliesAt(const QModelIndex& index) const
{
    FamilyAssociation assoc;
    if (!index.isValid())
    {
        return assoc;
    }

    const Document& doc = m_documentManager->document();
    ItemType type = itemTypeAt(index);
    QString id = idAt(index);

    switch (type)
    {
    case ItemType::Asset:
    {
        std::optional<EmergencyAsset> assetOpt = doc.findEmergencyAssetById(id);
        if (assetOpt)
        {
            for (const QString& personId : assetOpt->personIds())
            {
                QString familyId = doc.familyIdForPerson(personId);
                if (!familyId.isEmpty())
                {
                    assoc.relatedFamilyIds.insert(familyId);
                }
            }
        }
        break;
    }
    case ItemType::Person:
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

    // No contact points in emergency assets
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

QString EmergencyAssetModel::assetIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    // For asset nodes, return the node's own ID
    // For person nodes, return the parent asset's ID
    if (node->type == ItemType::Asset)
    {
        return node->id;
    }
    return node->assetId;
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

    const Document& doc = m_documentManager->document();
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

    int insertRow = node->children.size();
    beginInsertRows(index, insertRow, insertRow + itemCount - 1);

    // Phone
    if (!person->phone().isEmpty())
    {
        TreeNode* phoneNode = new TreeNode();
        phoneNode->type = ItemType::ContactDetail;
        phoneNode->id = node->id;
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
        altPhoneNode->id = node->id;
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
        emailNode->id = node->id;
        emailNode->assetId = node->assetId;
        emailNode->displayText = ContactIcons::Email + person->email();
        emailNode->parent = node;
        node->children.append(emailNode);
    }

    // Address (from family)
    if (hasAddress)
    {
        const Family& family = families[familyId];
        TreeNode* addrNode = new TreeNode();
        addrNode->type = ItemType::ContactDetail;
        addrNode->id = node->id;
        addrNode->assetId = node->assetId;
        addrNode->displayText = ContactIcons::Address + family.address().full();
        addrNode->parent = node;
        node->children.append(addrNode);
    }

    endInsertRows();
    node->contactsLoaded = true;
}

void EmergencyAssetModel::refreshFamilyDisplayText(const QString& familyId)
{
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

    // Walk tree and update affected person nodes
    for (int assetRow = 0; assetRow < m_assetNodes.size(); ++assetRow)
    {
        TreeNode* assetNode = m_assetNodes[assetRow];

        for (int personRow = 0; personRow < assetNode->children.size(); ++personRow)
        {
            TreeNode* personNode = assetNode->children[personRow];

            if (personNode->type == ItemType::Person && personIds.contains(personNode->id))
            {
                std::optional<Person> person = doc.findPersonById(personNode->id);
                if (person)
                {
                    personNode->displayText = person->displayName();

                    QModelIndex assetIndex = createIndex(assetRow, 0, assetNode);
                    QModelIndex personIndex = index(personRow, 0, assetIndex);
                    emit dataChanged(personIndex, personIndex);
                }
            }
        }
    }
}
