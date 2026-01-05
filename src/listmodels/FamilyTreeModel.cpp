#include "FamilyTreeModel.h"
#include "DocumentManager.h"
#include "Family.h"
#include "Filter.h"
#include "Person.h"

#include <QColor>
#include <QDebug>

#include <algorithm>

FamilyTreeModel::FamilyTreeModel(DocumentManager* documentManager,
                                 Filter* filter,
                                 QObject* parent)
    : QAbstractItemModel(parent)
    , m_documentManager(documentManager)
    , m_filter(filter)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &FamilyTreeModel::rebuild);
    connect(m_filter, &Filter::changed,
            this, &FamilyTreeModel::rebuild);
    rebuild();
}

FamilyTreeModel::~FamilyTreeModel()
{
    clearNodes();
}

void FamilyTreeModel::clearNodes()
{
    qDeleteAll(m_familyNodes);
    m_familyNodes.clear();
}

void FamilyTreeModel::rebuild()
{
    beginResetModel();

    clearNodes();
    m_familyIds.clear();

    const Document& doc = m_documentManager->document();
    const QHash<QString, Family>& families = doc.families();

    // Collect and filter family IDs
    QList<QString> ids;
    ids.reserve(families.size());

    for (auto it = families.begin(); it != families.end(); ++it)
    {
        if (m_filter->passes(doc, it.value()))
        {
            ids.append(it.key());
        }
    }

    // Sort by display name
    std::sort(ids.begin(), ids.end(), [&families](const QString& a, const QString& b) {
        return families.value(a).displayName().toLower()
             < families.value(b).displayName().toLower();
    });

    m_familyIds = ids;

    // Build tree nodes for each family
    for (int i = 0; i < m_familyIds.size(); ++i)
    {
        buildFamilyNode(i);
    }

    endResetModel();

    emit familyListChanged();
}

void FamilyTreeModel::buildFamilyNode(int familyIndex)
{
    const QString& familyId = m_familyIds.at(familyIndex);
    std::optional<Family> opt = m_documentManager->document().findFamilyById(familyId);
    if (!opt)
    {
        return;
    }

    const Family& family = *opt;

    // Create family node
    TreeNode* familyNode = new TreeNode();
    familyNode->type = RowType::Family;
    familyNode->familyIndex = familyIndex;
    familyNode->displayText = family.displayName();
    m_familyNodes.append(familyNode);

    // Add member nodes
    const QList<Person>& members = family.members();
    for (int memberIdx = 0; memberIdx < members.size(); ++memberIdx)
    {
        const Person& person = members.at(memberIdx);

        // Debug: dump birthday data for Flake family only
        if (person.surname() == "Flake")
        {
            const Birthday& bday = person.birthday();
            qDebug() << "Person:" << person.givenNames()
                     << "hasDate:" << bday.hasDate()
                     << "month:" << (bday.month().has_value() ? bday.month().value() : -1)
                     << "day:" << (bday.day().has_value() ? bday.day().value() : -1)
                     << "year:" << (bday.year().has_value() ? bday.year().value() : -1)
                     << "dateDisplay:" << person.birthDateDisplay();
        }

        TreeNode* memberNode = new TreeNode();
        memberNode->type = RowType::Member;
        memberNode->familyIndex = familyIndex;
        memberNode->memberIndex = memberIdx;
        memberNode->parent = familyNode;

        // Show given name if same surname, full name otherwise
        QString displayName = person.givenNames();
        if (!person.surname().isEmpty() && person.surname() != family.surname())
        {
            displayName = person.displayName();
        }
        memberNode->displayText = displayName;

        familyNode->children.append(memberNode);

        // Add member detail nodes (only for non-empty values)
        Phone phone = person.displayPhone();
        if (!phone.isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Phone;
            detailNode->displayText = tr("Phone: %1").arg(QString(phone));
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (!person.altPhone().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::AltPhone;
            detailNode->displayText = tr("Alt: %1").arg(QString(person.altPhone()));
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (!person.email().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Email;
            detailNode->displayText = tr("Email: %1").arg(person.email());
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (!person.callings().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Callings;
            detailNode->displayText = tr("Callings: %1").arg(person.callings().join(", "));
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        // Birthday: show date for everyone, append age for children
        QString birthDisplay = person.birthDateDisplay();
        if (!birthDisplay.isEmpty())
        {
            QString ageStr = person.ageDisplay();
            if (!ageStr.isEmpty())
            {
                birthDisplay += " " + ageStr;
            }

            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Age;
            detailNode->displayText = birthDisplay;
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        // If member has no details, add a "No contact info" placeholder
        if (memberNode->children.isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Phone;
            detailNode->displayText = tr("No contact info");
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }
    }

    // Add address node
    TreeNode* addressNode = new TreeNode();
    addressNode->type = RowType::Address;
    addressNode->familyIndex = familyIndex;
    addressNode->parent = familyNode;
    QString addressText = family.address().multiLine();
    addressNode->displayText = addressText.isEmpty() ? tr("No address") : addressText;
    familyNode->children.append(addressNode);

    // Add phone node
    TreeNode* phoneNode = new TreeNode();
    phoneNode->type = RowType::Phone;
    phoneNode->familyIndex = familyIndex;
    phoneNode->parent = familyNode;
    Phone familyPhone = family.displayPhone();
    phoneNode->displayText = familyPhone.isEmpty() ? tr("No phone") : QString(familyPhone);
    familyNode->children.append(phoneNode);

    // Add actions node
    TreeNode* actionsNode = new TreeNode();
    actionsNode->type = RowType::Actions;
    actionsNode->familyIndex = familyIndex;
    actionsNode->parent = familyNode;
    familyNode->children.append(actionsNode);
}

FamilyTreeModel::TreeNode* FamilyTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}

QModelIndex FamilyTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        // Top-level: family rows
        if (row >= 0 && row < m_familyNodes.size())
        {
            return createIndex(row, 0, m_familyNodes.at(row));
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

QModelIndex FamilyTreeModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // Find parent's row in its parent's children (or in m_familyNodes if root)
    if (parentNode->parent)
    {
        int row = parentNode->parent->children.indexOf(parentNode);
        return createIndex(row, 0, parentNode);
    }
    else
    {
        // Parent is a family node (top-level)
        int row = m_familyNodes.indexOf(parentNode);
        return createIndex(row, 0, parentNode);
    }
}

int FamilyTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return m_familyNodes.size();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return node->children.size();
    }

    return 0;
}

int FamilyTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool FamilyTreeModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_familyNodes.isEmpty();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return !node->children.isEmpty();
    }

    return false;
}

QVariant FamilyTreeModel::data(const QModelIndex& index, int role) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QVariant();
    }

    switch (role)
    {
        case Qt::DisplayRole:
            // Actions row doesn't display text (widget handles it)
            if (node->type == RowType::Actions)
            {
                return QVariant();
            }
            return node->displayText;

        case RowTypeRole:
            return QVariant::fromValue(node->type);

        case FamilyIdRole:
            if (node->familyIndex >= 0 && node->familyIndex < m_familyIds.size())
            {
                return m_familyIds.at(node->familyIndex);
            }
            return QVariant();

        case MemberIndexRole:
            return node->memberIndex;

        case DetailTypeRole:
            return QVariant::fromValue(node->detailType);

        case Qt::ForegroundRole:
            // Gray out placeholder text
            if (node->displayText == tr("No contact info")
                || node->displayText == tr("No address")
                || node->displayText == tr("No phone"))
            {
                return QColor(128, 128, 128);
            }
            return QVariant();

        default:
            return QVariant();
    }
}

QHash<int, QByteArray> FamilyTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[RowTypeRole] = "rowType";
    roles[FamilyIdRole] = "familyId";
    roles[MemberIndexRole] = "memberIndex";
    roles[DetailTypeRole] = "detailType";
    return roles;
}

QStringList FamilyTreeModel::familyIds() const
{
    return m_familyIds;
}

QString FamilyTreeModel::familyIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node && node->familyIndex >= 0 && node->familyIndex < m_familyIds.size())
    {
        return m_familyIds.at(node->familyIndex);
    }
    return QString();
}

QModelIndex FamilyTreeModel::indexForFamilyId(const QString& id) const
{
    int idx = m_familyIds.indexOf(id);
    if (idx >= 0 && idx < m_familyNodes.size())
    {
        return createIndex(idx, 0, m_familyNodes.at(idx));
    }
    return QModelIndex();
}

FamilyTreeModel::RowType FamilyTreeModel::rowTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return RowType::Family;
}
