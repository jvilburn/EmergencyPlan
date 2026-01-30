#include "EmergencyResourceModel.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyResource.h"
#include "Person.h"

#include <algorithm>

EmergencyResourceModel::EmergencyResourceModel(DocumentManager* documentManager,
                                                 ResponseArea area,
                                                 QObject* parent)
    : QAbstractItemModel(parent)
    , m_documentManager(documentManager)
    , m_area(area)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &EmergencyResourceModel::onDocumentChanged);
    rebuild();
}

EmergencyResourceModel::~EmergencyResourceModel()
{
    clearNodes();
}

void EmergencyResourceModel::clearNodes()
{
    qDeleteAll(m_resourceNodes);
    m_resourceNodes.clear();
}

bool EmergencyResourceModel::shouldRebuild(const DocumentChange& change) const
{
    switch (change.scope)
    {
    case ChangeScope::Full:
    case ChangeScope::EmergencyResource:
    case ChangeScope::Family:  // Person names are in families
        return true;
    default:
        return false;
    }
}

void EmergencyResourceModel::onDocumentChanged(const DocumentChange& change)
{
    if (shouldRebuild(change))
    {
        rebuild();
    }
}

void EmergencyResourceModel::rebuild()
{
    beginResetModel();

    clearNodes();

    const Document& doc = m_documentManager->document();
    QList<EmergencyResource> resources = doc.emergencyResourcesByArea(m_area);

    // Sort resources by name
    std::sort(resources.begin(), resources.end(),
              [](const EmergencyResource& a, const EmergencyResource& b)
              { return a.name().toLower() < b.name().toLower(); });

    for (const EmergencyResource& resource : resources)
    {
        // Create resource node
        TreeNode* resourceNode = new TreeNode();
        resourceNode->type = ItemType::Resource;
        resourceNode->id = resource.id();
        resourceNode->displayText = QString("%1 (%2)")
            .arg(resource.name())
            .arg(resource.personIds().size());

        // Collect and sort people in this resource
        QList<QPair<QString, QString>> people;  // (personId, displayName)
        for (const QString& personId : resource.personIds())
        {
            std::optional<Person> person = doc.findPersonById(personId);
            if (person)
            {
                people.append({personId, person->displayName()});
            }
        }
        std::sort(people.begin(), people.end(),
                  [](const QPair<QString, QString>& a, const QPair<QString, QString>& b)
                  { return a.second.toLower() < b.second.toLower(); });

        // Create person nodes as children of resource
        for (const QPair<QString, QString>& personData : people)
        {
            TreeNode* personNode = new TreeNode();
            personNode->type = ItemType::Person;
            personNode->id = personData.first;
            personNode->resourceId = resource.id();
            personNode->displayText = personData.second;
            personNode->parent = resourceNode;
            resourceNode->children.append(personNode);
        }

        m_resourceNodes.append(resourceNode);
    }

    endResetModel();
}

EmergencyResourceModel::TreeNode* EmergencyResourceModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}

QModelIndex EmergencyResourceModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        // Top-level: resource rows
        if (row >= 0 && row < m_resourceNodes.size())
        {
            return createIndex(row, 0, m_resourceNodes.at(row));
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

QModelIndex EmergencyResourceModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // Parent is always a resource node (top-level)
    int row = m_resourceNodes.indexOf(parentNode);
    if (row >= 0)
    {
        return createIndex(row, 0, parentNode);
    }

    return QModelIndex();
}

int EmergencyResourceModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        // Root: number of resources
        return m_resourceNodes.size();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return node->children.size();
    }

    return 0;
}

int EmergencyResourceModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant EmergencyResourceModel::data(const QModelIndex& index, int role) const
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
    case ResourceIdRole:
        return node->resourceId;
    default:
        return QVariant();
    }
}

QString EmergencyResourceModel::idAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->id;
    }
    return QString();
}

EmergencyResourceModel::ItemType EmergencyResourceModel::itemTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return ItemType::Resource;
}

QString EmergencyResourceModel::resourceIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QString();
    }

    // For resource nodes, return the node's own ID
    // For person nodes, return the parent resource's ID
    if (node->type == ItemType::Resource)
    {
        return node->id;
    }
    return node->resourceId;
}
