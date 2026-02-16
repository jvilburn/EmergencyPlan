#include "PersonTreeModel.h"
#include "ContactIcons.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Family.h"
#include "Filter.h"
#include "Person.h"

#include <QColor>

#include <algorithm>

PersonTreeModel::PersonTreeModel(DocumentManager* documentManager,
                                 Filter* filter,
                                 QObject* parent)
    : BaseTreeModel(parent)
    , m_documentManager(documentManager)
    , m_filter(filter)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &PersonTreeModel::onDocumentChanged);
    if (m_filter)
    {
        connect(m_filter, &Filter::changed, this, &PersonTreeModel::rebuild);
    }
    rebuild();
}

PersonTreeModel::~PersonTreeModel()
{
    clearNodes();
}

void PersonTreeModel::clearNodes()
{
    qDeleteAll(m_personNodes);
    m_personNodes.clear();
}

void PersonTreeModel::onDocumentChanged(const DocumentChange& change)
{
    // Full rebuild for structural changes
    if (change.action == ChangeAction::Full || change.familyId)
    {
        rebuild();
    }
}

void PersonTreeModel::rebuild()
{
    beginResetModel();

    clearNodes();
    m_personData.clear();

    const Document& doc = m_documentManager->document();
    const QHash<FamilyId, Family>& families = doc.families();

    // Collect all persons with their family IDs
    struct PersonInfo
    {
        PersonId personId;
        FamilyId familyId;
        QString displayName;
    };
    QList<PersonInfo> allPersons;

    for (auto it = families.begin(); it != families.end(); ++it)
    {
        const Family& family = it.value();
        for (const Person& person : family.members())
        {
            // Apply filter if set
            if (m_filter && !m_filter->passes(doc, person))
            {
                continue;
            }
            allPersons.append({person.id(), family.id(), person.displayName()});
        }
    }

    // Sort by display name
    std::sort(allPersons.begin(), allPersons.end(),
              [](const PersonInfo& a, const PersonInfo& b)
              { return a.displayName.toLower() < b.displayName.toLower(); });

    // Build m_personData and tree nodes
    for (int i = 0; i < allPersons.size(); ++i)
    {
        const PersonInfo& info = allPersons.at(i);
        m_personData.append({info.personId, info.familyId});
        buildPersonNode(i);
    }

    endResetModel();
}

void PersonTreeModel::buildPersonNode(int personIndex)
{
    const QPair<PersonId, FamilyId>& data = m_personData.at(personIndex);
    const PersonId& personId = data.first;
    const FamilyId& familyId = data.second;

    const Document& doc = m_documentManager->document();
    std::optional<Person> personOpt = doc.findPersonById(personId);
    if (!personOpt)
    {
        return;
    }

    const Person& person = *personOpt;

    // Create person node
    TreeNode* personNode = new TreeNode();
    personNode->type = ItemType::Person;
    personNode->personId = personId;
    personNode->familyId = familyId;
    personNode->displayText = person.displayName();
    m_personNodes.append(personNode);

    // Add phone detail
    if (!person.phone().isEmpty())
    {
        TreeNode* detailNode = new TreeNode();
        detailNode->type = ItemType::ContactDetail;
        detailNode->personId = personId;
        detailNode->familyId = familyId;
        detailNode->displayText = ContactIcons::Phone + QString(person.phone());
        detailNode->parent = personNode;
        personNode->children.append(detailNode);
    }

    // Add alt phone detail
    if (!person.altPhone().isEmpty())
    {
        TreeNode* detailNode = new TreeNode();
        detailNode->type = ItemType::ContactDetail;
        detailNode->personId = personId;
        detailNode->familyId = familyId;
        detailNode->displayText = ContactIcons::Phone + QString(person.altPhone()) + tr(" (alt)");
        detailNode->parent = personNode;
        personNode->children.append(detailNode);
    }

    // Add email detail
    if (!person.email().isEmpty())
    {
        TreeNode* detailNode = new TreeNode();
        detailNode->type = ItemType::ContactDetail;
        detailNode->personId = personId;
        detailNode->familyId = familyId;
        detailNode->displayText = ContactIcons::Email + person.email();
        detailNode->parent = personNode;
        personNode->children.append(detailNode);
    }

    // Add address from family
    const QHash<FamilyId, Family>& families = doc.families();
    if (families.contains(familyId))
    {
        const Family& family = families[familyId];
        if (!family.address().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = ItemType::ContactDetail;
            detailNode->personId = personId;
            detailNode->familyId = familyId;
            detailNode->displayText = ContactIcons::Address + family.address().full();
            detailNode->parent = personNode;
            personNode->children.append(detailNode);
        }
    }

    // If no contact details, add placeholder
    if (personNode->children.isEmpty())
    {
        TreeNode* detailNode = new TreeNode();
        detailNode->type = ItemType::ContactDetail;
        detailNode->personId = personId;
        detailNode->familyId = familyId;
        detailNode->displayText = tr("No contact info");
        detailNode->parent = personNode;
        personNode->children.append(detailNode);
    }
}

PersonTreeModel::TreeNode* PersonTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}

QModelIndex PersonTreeModel::index(int row, int column, const QModelIndex& parent) const
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

QModelIndex PersonTreeModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // Parent is a person node (top-level)
    int row = m_personNodes.indexOf(parentNode);
    if (row >= 0)
    {
        return createIndex(row, 0, parentNode);
    }

    return QModelIndex();
}

int PersonTreeModel::rowCount(const QModelIndex& parent) const
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

int PersonTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool PersonTreeModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_personNodes.isEmpty();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return !node->children.isEmpty();
    }

    return false;
}

QVariant PersonTreeModel::data(const QModelIndex& index, int role) const
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

    case Qt::ForegroundRole:
        // Gray out placeholder text
        if (node->displayText == tr("No contact info"))
        {
            return QColor(128, 128, 128);
        }
        return QVariant();

    default:
        return QVariant();
    }
}

ItemType PersonTreeModel::itemTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return ItemType::Invalid;
}

SelectionKey PersonTreeModel::selectionKeyAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return SelectionKey::from(QString());
    }

    // Contact detail nodes delegate to parent
    if (node->type == ItemType::ContactDetail)
    {
        return selectionKeyAt(index.parent());
    }
    return SelectionKey::from(node->personId);
}

PersonId PersonTreeModel::personIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->personId;
    }
    return PersonId::from(QString());
}

FamilyId PersonTreeModel::familyIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->familyId;
    }
    return FamilyId::from(QString());
}

QModelIndex PersonTreeModel::indexForPersonId(const PersonId& personId) const
{
    for (int i = 0; i < m_personNodes.size(); ++i)
    {
        if (m_personNodes.at(i)->personId == personId)
        {
            return createIndex(i, 0, m_personNodes.at(i));
        }
    }
    return QModelIndex();
}
